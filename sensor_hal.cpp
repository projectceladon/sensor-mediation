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
    {"Accelerometer",
     "Intel",
     1,
     0,
     SENSOR_TYPE_ACCELEROMETER,
     1000,
     1.52e-5,
     0.0,
     0,
     0,
     0,
     "android.sensor.accelerometer",
     "",
     20000,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"incli_3d",
     "Intel",
     1,
     1,
     SENSOR_TYPE_LINEAR_ACCELERATION,
     1000,
     0.1,
     0.0,
     0,
     0,
     0,
     "android.sensor.inclinometer",
     "",
     20000,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"gravity",
     "Intel",
     1,
     2,
     SENSOR_TYPE_GRAVITY,
     1000,
     0.1,
     0.0,
     0,
     0,
     0,
     "android.sensor.gravity",
     "",
     20000,
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
     0.0,
     0,
     0,
     0,
     "android.sensor.dev_rotation",
     "",
     20000,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"magn_3d",
     "Intel",
     1,
     4,
     SENSOR_TYPE_MAGNETIC_FIELD,
     1000,
     0.1,
     0.0,
     0,
     0,
     0,
     "android.sensor.magn_3d",
     "",
     20000,
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
     0.0,
     0,
     0,
     0,
     "android.sensor.geomagnetic_orientation",
     "",
     20000,
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
     0.0,
     0,
     0,
     0,
     "android.sensor.relative_orientation",
     "",
     20000,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"gyro_3d",
     "Intel",
     1,
     7,
     SENSOR_TYPE_GYROSCOPE,
     100,
     0.1,
     0.0,
     0,
     0,
     0,
     "android.sensor.gyro_3d",
     "",
     20000,
     SENSOR_FLAG_CONTINUOUS_MODE,
     {},
    },
    {"Ambient light sensor",
     "Intel",
     1,
     8,
     SENSOR_TYPE_LIGHT,
     100,
     0.1,
     0.0,
     0,
     0,
     0,
     "android.sensor.als",
     "",
     20000,
     SENSOR_FLAG_ON_CHANGE_MODE,
     {},
    },
};

static int open_sensors(const struct hw_module_t* module, const char* id,
            struct hw_device_t** device);

static int sensors__get_sensors_list(struct sensors_module_t* module,
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
    .get_sensors_list = sensors__get_sensors_list,
};

static int poll__poll(struct sensors_poll_device_t* dev,
                    sensors_event_t* data, int count)
{
    int evCount = 0;

    UNUSED(dev);
    if (count < 1)
        return -EINVAL;
    if (is_meta_data_pending) {
        for (int i = 0; i < MAX_SENSOR; i++) {
            is_meta_data_pending = false;
            data[i].version = META_DATA_VERSION;
            data[i].sensor = 0;
            data[i].type = SENSOR_TYPE_META_DATA;
            data[i].reserved0 = 0;
            data[i].timestamp = 0;
            data[i].meta_data.sensor = i;
            data[i].meta_data.what = META_DATA_FLUSH_COMPLETE;
        }

        evCount = MAX_SENSOR;
    } else {
        evCount = iioc.getPollData(data);
    }

    return evCount;
}

static int poll__activate(struct sensors_poll_device_t *dev,
                                    int handle, int enabled)
{
    UNUSED(dev);
    UNUSED(handle);
    UNUSED(enabled);

    return 0;
}

static int poll__setDelay(struct sensors_poll_device_t *dev,
                                    int handle, int64_t ns)
{
    UNUSED(dev);
    UNUSED(handle);
    UNUSED(ns);

    return 0;
}

/* Batch mode is unsupported for thermal sensor */
static int poll__batch(struct sensors_poll_device_1* dev,
        int sensor_handle, int flags,
        int64_t sampling_period_ns, int64_t max_report_latency_ns)
{
    UNUSED(dev);
    UNUSED(sensor_handle);
    UNUSED(flags);
    UNUSED(sampling_period_ns);
    UNUSED(max_report_latency_ns);

    return 0;
}

static int poll__flush(struct sensors_poll_device_1* dev, int handle)
{
    UNUSED(dev);
    UNUSED(handle);
    is_meta_data_pending = true;

    return 0;
}

/* Nothing to be cleared on close */
static int poll__close(struct hw_device_t *dev)
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
    dev.common.close = poll__close;
    dev.activate = poll__activate;
    dev.poll = poll__poll;
    dev.batch = poll__batch;
    dev.flush = poll__flush;
    *device = &dev.common;

    return 0;
}
