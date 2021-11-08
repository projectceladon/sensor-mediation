/*
 * Copyright (c) 2020 Intel Corporation
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

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <utils/Log.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h>

#include "iio-client.h"

#ifndef UNUSED
#define UNUSED(x)    (void)(x)
#endif

static bool is_meta_data_pending = false;
static iioClient iioc;
static const struct sensor_t sSensorList[MAX_SENSOR] = {
    {"Accelerometer", // name
     "Intel",         // vendor
     1,               // version
     0,               // handle
     SENSOR_TYPE_ACCELEROMETER, // type
     100.0f,            // maxrange
     1.1926889E-4,    // resolution
     0.001f,          // Power
     kDefaultMinDelayUs,         // minDelay in us
     0,               // fifoReservedEventCount
     0,               // fifoMaxEventCount
     SENSOR_STRING_TYPE_ACCELEROMETER, // stringType
     "",              // requiredPermission
     kDefaultMaxDelayUs,          // maxDelay
     SENSOR_FLAG_CONTINUOUS_MODE, // flags
     {},              // reserved[2]
    },
    {"incli_3d",
     "Intel",
     1,
     1,
     SENSOR_TYPE_LINEAR_ACCELERATION,
     1000,
     0.1f,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_LINEAR_ACCELERATION,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"gravity",
     "Intel",
     1,
     2,
     SENSOR_TYPE_GRAVITY,
     1000,
     0.1f,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_GRAVITY,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"dev_rotation",
     "Intel",
     1,
     3,
     SENSOR_TYPE_ROTATION_VECTOR,
     1000,
     0.1,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_ROTATION_VECTOR,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"magn_3d",
     "Intel",
     1,
     4,
     SENSOR_TYPE_MAGNETIC_FIELD,
     1300.0f,
     0.01,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_MAGNETIC_FIELD,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"geomagnetic_orientation",
     "Intel",
     1,
     5,
     SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
     100,
     0.1,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"relative_orientation",
     "Intel",
     1,
     6,
     SENSOR_TYPE_GAME_ROTATION_VECTOR,
     100,
     0.1,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_GAME_ROTATION_VECTOR,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"gyro_3d",
     "Intel",
     1,
     7,
     SENSOR_TYPE_GYROSCOPE,
     1000.0f,
     0.048852537,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_GYROSCOPE,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"Ambient light sensor",
     "Intel",
     1,
     8,
     SENSOR_TYPE_LIGHT,
     43000.0f,
     1.0f,
     0.001f,
     kDefaultMinDelayUs,
     0,
     0,
     SENSOR_STRING_TYPE_LIGHT,
     "",
     kDefaultMaxDelayUs,
     SENSOR_FLAG_ON_CHANGE_MODE,
     {},
    },
};

static int open_sensors(const struct hw_module_t* module, const char* id,
            struct hw_device_t** device);

static int get_sensors_list(struct sensors_module_t* module,
            struct sensor_t const** list)
{
    UNUSED(module);
    *list = sSensorList;

    return MAX_SENSOR;
}

static struct hw_module_methods_t sensors_module_methods = {
    .open = open_sensors
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 3,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "Intel sensor-mediation module",
        .author = "Intel",
        .methods = &sensors_module_methods,
        .dso = 0,
        .reserved = {},
    },
    .get_sensors_list = get_sensors_list,
};

static int poll(struct sensors_poll_device_t* dev,
                    sensors_event_t* data, int count)
{
    int evCount = 0;
    UNUSED(dev);
    if (count < 1)
        return -EINVAL;

    if (is_meta_data_pending) {
        for (int i = 0; i < MAX_SENSOR; i++) {
            data[i].version = META_DATA_VERSION;
            data[i].sensor = 0;
            data[i].type = SENSOR_TYPE_META_DATA;
            data[i].reserved0 = 0;
            data[i].timestamp = iioc.get_timestamp(CLOCK_BOOTTIME);
            data[i].meta_data.sensor = i;
            data[i].meta_data.what = META_DATA_FLUSH_COMPLETE;
        }
        evCount = MAX_SENSOR;
        is_meta_data_pending = false;
        ALOGD("Flushed %d sensors", evCount);
    } else {
        evCount = iioc.poll(data, count);
    }

    return evCount;
}

static int activate(struct sensors_poll_device_t *dev,
                    int handle, int enabled)
{
    UNUSED(dev); //UNUSED
    return iioc.activate(handle, enabled);
}

static int setDelay(struct sensors_poll_device_t *dev,
                                    int handle, int64_t ns)
{
    UNUSED(dev);
    ALOGD("setDelay: handle(%d), ns(%lld) ####", handle, (long long) ns);
    return 0;
}

/* Batch mode is unsupported for thermal sensor */
static int batch(struct sensors_poll_device_1* dev,
        int sensor_handle, int flags,
        int64_t sampling_period_ns, int64_t max_report_latency_ns)
{
    UNUSED(dev);
    UNUSED(flags);
    iioc.batch(sensor_handle, sampling_period_ns, max_report_latency_ns);
    return 0;
}

static int flush(struct sensors_poll_device_1* dev, int handle)
{
    UNUSED(dev);
    UNUSED(handle);
    is_meta_data_pending = true;
    return 0;
}

/* Nothing to be cleared on close */
static int close(struct hw_device_t *dev)
{
    UNUSED(dev);
    return 0;
}

static int open_sensors(const struct hw_module_t* module, const char* id,
                    struct hw_device_t** device)
{
    static struct sensors_poll_device_1 dev;

    UNUSED(id);
    dev.common.tag = HARDWARE_DEVICE_TAG;
    dev.common.version = SENSORS_DEVICE_API_VERSION_1_3;
    dev.common.module = const_cast<hw_module_t *>(module);
    dev.activate = activate;
    dev.poll = poll;
    dev.batch = batch;
    dev.setDelay = setDelay;
    dev.flush = flush;
    dev.common.close = close;
    *device = &dev.common;

    return 0;
}
