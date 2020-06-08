/*
 * Copyright (c) 2020 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <linux/limits.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <log/log.h>
#include <iostream>

#include "iio-client.h"

struct idMap iM[MAX_SENSOR] = {{"accel_3d", 0, SENSOR_TYPE_ACCELEROMETER},
        {"incli_3d", 1, SENSOR_TYPE_LINEAR_ACCELERATION},
        {"gravity", 2, SENSOR_TYPE_GRAVITY},
        {"dev_rotation", 3, SENSOR_TYPE_ROTATION_VECTOR},
        {"magn_3d", 4, SENSOR_TYPE_MAGNETIC_FIELD},
        {"geomagnetic_orientation", 5, SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR},
        {"relative_orientation", 6, SENSOR_TYPE_GAME_ROTATION_VECTOR},
        {"gyro_3d", 7, SENSOR_TYPE_GYROSCOPE},
        {"als", 8, SENSOR_TYPE_LIGHT}};

iioClient::iioClient()
{
    init();
    sensorCount = 0;
    ctx = NULL;
    sensorList = NULL;
}

iioClient::~iioClient()
{
    if (ctx)
        iio_context_destroy(ctx);

    if (sensorList)
        free(sensorList);
}

int iioClient::init(void)
{
    char value[PROPERTY_VALUE_MAX] = {0};

    ctx = NULL;
    sensorCount = 0;
    property_get("ipaddr", value, " ");
    ctx = iio_create_network_context(value);
    if (!ctx) {
        ALOGE("Sensor: Error in Initializing IIO Client with N/W backend\n");
        return -1;
    }

    int nb_devices = iio_context_get_devices_count(ctx);
    for (int i = 0; i < nb_devices; i++) {
        const struct iio_device *dev = iio_context_get_device(ctx, i);
        if (!dev) {
            ALOGE("Sensor device context is NULL for %d \n", i);
            continue;
        }

        unsigned int nb_channels = iio_device_get_channels_count(dev);
        if (nb_channels > 0) {
            sensorCount++;
        }
    }

    if (!sensorCount) {
        ALOGE("Sensor:  Found zero sensors");
        return -1;
    } else {
        ALOGI("Sensor: Sensor Count: %u\n", sensorCount);
    }

    sensorList = new sensor_t[sensorCount];
    int j = 0;
    for (int i = 0; i < sensorCount; i++, j++) {
        const struct iio_device *dev = iio_context_get_device(ctx, i);
        if (!dev) {
            ALOGE("Sensor device context is NULL for %d \n", i);
            j -= 1;
            continue;
        }

        unsigned int nb_channels = iio_device_get_channels_count(dev);
        int index;

        sensorList[j].name = iio_device_get_name(dev);
        index = compare(sensorList[j].name);
        if (index < 0) {
            j -= 1;
            ALOGE("Sensor type not found name: %s \n", sensorList[j].name);
            continue;
        }

        sensorList[j].vendor = "Intel";
        sensorList[j].version = 1;
        sensorList[j].handle = iM[index].id;
        sensorList[j].type = iM[index].type;
        sensorList[j].maxRange = 100;
        sensorList[j].resolution = 0.1;
        sensorList[j].power = 0.0;
        sensorList[j].minDelay = 0;
        sensorList[j].fifoReservedEventCount = 0;
        sensorList[j].fifoMaxEventCount = 0;
        sensorList[j].stringType = "android.sensor";
        sensorList[j].requiredPermission = "";
        sensorList[j].maxDelay = 20000;
        sensorList[j].flags = SENSOR_FLAG_ON_CHANGE_MODE;
    }

    sensorCount = j;

    return 0;
}

sensor_t * iioClient::getSensorList(void)
{
    return sensorList;
}

int64_t iioClient::get_timestamp(clockid_t clock_id)
{
    struct timespec ts = {0, 0};

    if (!clock_gettime(clock_id, &ts))
        return 1000000000LL * ts.tv_sec + ts.tv_nsec;
    else /* in this case errno is set appropriately */
        return -1;
}

int iioClient::compare(const char *name)
{
    for (int i = 0; i <  MAX_SENSOR; i++)
        if (!strcmp(name, iM[i].name)) {
            return i;
        }

    return -1;
}

/*
 * Receives sensor data from server
 */
int iioClient::getPollData(sensors_event_t* data)
{
    while (!sensorCount || !ctx) {
        sleep(1);
        init();
    }

    int k = 0;
    for (int i = 0; i < sensorCount; i++, k++) {
        unsigned type;
        const struct iio_device *dev = iio_context_get_device(ctx, i);
        if (!dev) {
            ALOGE("Failed to get sensor device %d\n", i);
            k -= 1;
            continue;
        }

        unsigned int nb_channels = iio_device_get_channels_count(dev);
        int index = compare(iio_device_get_name(dev));

        if (index < 0) {
            k -= 1;
            continue;
        }

        data[k].sensor = iM[index].id;
        data[k].type = iM[index].type;
        data[k].version = sensorList[i].version;
        data[k].timestamp = get_timestamp(CLOCK_MONOTONIC);
        for (int j = 0; j < nb_channels; j++) {
            struct iio_channel *ch = iio_device_get_channel(dev, j);
            if (!ch) {
                k -= 1;
                continue;
            }

            const char *attr = iio_channel_get_attr(ch, 2);
            if (!attr) {
                k -= 1;
                continue;
            }

            char buf[1024];

            iio_channel_attr_read(ch, attr, buf, sizeof(buf));
            data[k].data[j] = strtof(buf, NULL);
        }
    }

    return k;
}
