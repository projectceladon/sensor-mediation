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

struct idMap {
    const char *name;
    int id;
    int type;
};

class iioClient {
 public:
    iioClient();
    ~iioClient();
    int getPollData(sensors_event_t *);

 private:
    sensor_t *sensorList;
    volatile int sensorCount;
    struct iio_context *ctx;
    int compare(const char *);
    int64_t get_timestamp(clockid_t);
    sensor_t *getSensorList(void);
    int init(void);
};
#endif  /*IIO_CLIENT_H_*/
