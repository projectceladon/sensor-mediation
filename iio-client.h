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

#ifndef IIO_CLIENT_H_
#define IIO_CLIENT_H_

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>
#include <utils/Atomic.h>
#include <hardware/hardware.h>
#include <hardware/sensors.h>
#include <hardware/sensors-base.h>

#include "custom-libiio-client/iio.h"

#define MAX_SENSOR 9
#define MAX_CHANNEL 3
#define kDefaultMinDelayUs (10 * 1000)
#define kDefaultMaxDelayUs (200 * 1000)

struct idMap {
    const char *name;
    int id;
    int type;
};

struct event_info {
    const char* name;
    int handle;
    sensors_event_t* event;
    int data_count;
    float scale;
};

struct iioclient_device {
    const char *name;
    const struct iio_device *dev;
    int type;
    double scale;
    int raw_channel_count;
    struct iio_channel *channel_raw[10];
    struct iio_channel *channel_frequency;
    float data[16];
    unsigned int nb_channels;
    const char *frequency_channel;
    bool is_initialized;
    bool is_enabled;
    bool is_activate_pending;
    bool activation_pending_state;
    bool is_batch_pending;
    int64_t sampling_period_us;
};

class iioClient {
 public:
    iioClient();
    ~iioClient();
    volatile int sensorCount;
    int poll(sensors_event_t *data, int count);
    int activate(int handle, bool enabled);
    int batch(int handle, int64_t sampling_period_ns, int64_t max_report_latency_ns);
    int64_t get_timestamp(clockid_t);

 private:
    bool is_iioc_initialized;
    int64_t mSamplingPeriodUs;
    struct iio_context *ctx;
    struct iioclient_device devlist[MAX_SENSOR];
    bool init(void);
    int get_android_sensor_id_by_name(const char *name);
};
#endif  /*IIO_CLIENT_H_*/
