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
#include <thread>

#include "iio-client.h"

/**
 * id_map, holds list of sensors information to map during iio initialization.
 * id from sensorList@sensor_hal.cpp and id@id_map should be matched
 */
struct idMap id_map[MAX_SENSOR] = {{"accel_3d", 0, SENSOR_TYPE_ACCELEROMETER},
        {"incli_3d", 1, SENSOR_TYPE_LINEAR_ACCELERATION},
        {"gravity", 2, SENSOR_TYPE_GRAVITY},
        {"dev_rotation", 3, SENSOR_TYPE_ROTATION_VECTOR},
        {"magn_3d", 4, SENSOR_TYPE_MAGNETIC_FIELD},
        {"geomagnetic_orientation", 5, SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR},
        {"relative_orientation", 6, SENSOR_TYPE_GAME_ROTATION_VECTOR},
        {"gyro_3d", 7, SENSOR_TYPE_GYROSCOPE},
        {"als", 8, SENSOR_TYPE_LIGHT}};

static int active_sensor_count;
/**
 * constructor called when iioclient object created.
 * initialize member variables to default state.
 */
iioClient::iioClient() {
    sensorCount = 0;
    ctx = NULL;
    is_iioc_initialized = false;
}

/**
 * destructor called when iioclient object deleted.
 * and destroy iio client if already initialized.
 */
iioClient::~iioClient() {
    if (ctx)
        iio_context_destroy(ctx);
}

int64_t iioClient::get_timestamp(clockid_t clock_id) {
    struct timespec ts = {0, 0};

    if (!clock_gettime(clock_id, &ts))
        return 1000000000LL * ts.tv_sec + ts.tv_nsec;

    /* in this case errno is set appropriately */
    return -1;
}

/**
 * get_android_sensor_id_by_name is an helper function to map
 * iio sensor with android sensor list. return -1 when sensor
 * not found otherwise return sensor handle.
 */
int iioClient::get_android_sensor_id_by_name(const char *name) {
    for (int index = 0; index <  MAX_SENSOR; index++) {
        if (!strcmp(name, id_map[index].name)) {
            return index;
        }
    }

    /* in this case appropriate sensor not found */
    return -1;
}

/**
 * read_sensor_data thread is a deferred call to read sensor data.
 * this api is implemented to collect active sensor data from iiod.
 */
int read_sensor_data_thread(struct iioclient_device *devlist) {
    while(true) {
        /* wait if active sensors count is zero
         * improve cpu utilization and power cycle */
        if(!active_sensor_count) {
            usleep(10*1000); // 10ms
            continue;
        }

        for (int id = 0; id < MAX_SENSOR; id++) {
            if (!(devlist[id].is_initialized && devlist[id].is_enabled))
                continue;

            for (int index = 0; index < devlist[id].raw_channel_count; index++) {
                struct iio_channel *channel = devlist[id].channel_raw[index];
                char buf[1024] = {0};
                if (iio_channel_attr_read(channel, "raw", buf, sizeof(buf)) > 0)
                    devlist[id].data[index] = strtof(buf, NULL) * devlist[id].scale;
            }

        }
    }

    ALOGE("error: exit read_sensor_data_thread");
    return 0;
}

/**
 * iioClient:init, establish network backend connection with iiod server
 * and initialize raw channel, freq channel & read sensorscale.
 * return true on success else fail
 */
bool iioClient::init(void) {
    sensorCount = 0;
    ctx = NULL;
    active_sensor_count = 0;
    is_iioc_initialized = false;

    /* Read IP address from vendor property */
    char value[PROPERTY_VALUE_MAX] = {0};
    property_get("vendor.intel.ipaddr", value, " ");

    /* Create IIO context */
    ctx = iio_create_network_context(value);
    if (!ctx) {
        ALOGW("Warning: retrying sensor initialization with N/W backend.");
        return false;
    }

    unsigned int nb_iio_devices = iio_context_get_devices_count(ctx);
    for (int i = 0; i < nb_iio_devices; i++) {
        const struct iio_device *device = iio_context_get_device(ctx, i);
        /* Skip device with null pointer */
        if (!device)
            continue;

        const char *sensor_name = iio_device_get_name(device);
	int handle = get_android_sensor_id_by_name(sensor_name);
        /* Skip device with invalid id/handle */
        if (handle < 0)
            continue;
		
        bool scale_found = false;
        unsigned int nb_channels = iio_device_get_channels_count(device);
        /* Skip device with no channels */
        if (!nb_channels)
            continue;

        for (int ch_index = 0; ch_index < nb_channels; ch_index++) {
            struct iio_channel *channel = iio_device_get_channel(device, ch_index);
	    if (!iio_channel_is_output(channel)) {
                unsigned int attrs_count = iio_channel_get_attrs_count(channel);
                for (int attr_index = 0; attr_index < attrs_count; attr_index++) {
                    const char* attr_name = iio_channel_get_attr(channel, attr_index);
                    if (attr_name && !strcmp(attr_name, "raw")) {
                        /* get raw data channels */
                        devlist[handle].raw_channel_count = ch_index + 1;
                        devlist[handle].channel_raw[ch_index] = channel;
                    } else if (!scale_found && attr_name && !strcmp(attr_name, "scale")) {
                        char buf[1024] = {0};
                        /* reading scale */
                        if (iio_channel_attr_read(channel, "scale", buf, sizeof(buf)) > 0) {
                            devlist[handle].scale = strtod(buf, NULL);
                            scale_found = true;
                        }
                    } else if (!scale_found && attr_name && !strcmp(attr_name, "sampling_frequency")) {
                        /**
                         * channel name might be "sampling_frequency" or "frequency"
                         * find existing channel frequency.
                         */
                        devlist[handle].channel_frequency = channel;
                   }
                }
            }
        }

        /* Initialize & Map IIO sensor devices with Android sensor devices */
        devlist[handle].dev = device;
        devlist[handle].name = sensor_name;
        devlist[handle].nb_channels = nb_channels;
        devlist[handle].type = id_map[handle].type;
        devlist[handle].is_initialized = true;
        devlist[handle].sampling_period_us = kDefaultMinDelayUs;

        sensorCount++;
        if (sensorCount > MAX_SENSOR)
            break;
	}

    /* Destroy iio context if sensor count = zero */
    if (!sensorCount) {
        if (ctx)
            iio_context_destroy(ctx);
        return false;
    }
    
    ALOGE("Sensor: Initialized IIO Client with N/W backend, sensor_count(%u)", sensorCount);
    for (int id = 0; id < MAX_SENSOR; id++) {
        /**
         * Callback -> Activation/Deactivation
         * activate/deactivate might have skipped during initialization.
         * check and call activate based on activate pending status.
         */
        if (devlist[id].is_activate_pending) {
            activate(id, devlist[id].activation_pending_state);
            devlist[id].is_activate_pending = false;
        }

        /**
         * Callback -> Batch
         * batch call might have skipped during initialization.
         * check and call activate based on batch pending status.
         */ 
         if (devlist[id].is_batch_pending) {
             batch(id, devlist[id].sampling_period_us * 1000,
                       kDefaultMinDelayUs * 1000);
             devlist[id].is_batch_pending = false;
         }
    }

    is_iioc_initialized = true;
    static std::thread thread_object(read_sensor_data_thread, devlist);
    return true;
}

/**
 * Write an array of sensor_event_t to data. The size of the
 * available buffer is specified by count. Returns number of
 * valid sensor_event_t.
 */
int iioClient::poll(sensors_event_t* data, int count) {
    int event_count = 0;
    /**
     * Incase sensor not initialized, initialize it
     * once before use
     */
    if (!is_iioc_initialized && !init()) {
        sleep(1);
        return event_count;
    }
    if (!active_sensor_count) {
        usleep(10 * 1000);
        data[event_count].version = META_DATA_VERSION;
        data[event_count].sensor = 0;
        data[event_count].type = SENSOR_TYPE_META_DATA;
        data[event_count].reserved0 = 0;
        data[event_count].timestamp = get_timestamp(CLOCK_BOOTTIME);
        data[event_count].meta_data.sensor = 0;
        data[event_count].meta_data.what = META_DATA_FLUSH_COMPLETE;
        return (event_count + 1);
    }

    /* Collect sensor data from active sensor list */
    for (int id = 0; id < MAX_SENSOR; id++) {
        if (!(devlist[id].is_initialized && devlist[id].is_enabled)) {
            continue;
        }

        /**
         * Strictly eventcount should not exceed count,
         * incase it happens which leads to memory leak
         * and buffer corruption.
         */
        if (event_count > count) {
            return event_count;
        }

        int ts = devlist[id].sampling_period_us;
        usleep(ts-1200);

        for (int index = 0; index < devlist[id].raw_channel_count; index++)
            data[event_count].data[index] = devlist[id].data[index];

        /* Update sensor data events */
        data[event_count].sensor = id_map[id].id;
        data[event_count].type = id_map[id].type;
        data[event_count].version = 1;
        data[event_count].timestamp = get_timestamp(CLOCK_BOOTTIME);
 
        event_count++;
    }

    return event_count;
}

/**
 * Activate/de-activate one sensor.
 * sensor_handle is the handle of the sensor to change.
 * enabled set to 1 to enable, or 0 to disable the sensor.
 */
int iioClient::activate(int handle, bool enabled) {
    if ((handle < 0) || (handle > MAX_SENSOR)) {
        ALOGE("ERROR: activate(%d) Sensor hadle(%d) is out of range", enabled, handle);
        return 0;
    }

    /* Flag the state*/
    devlist[handle].is_enabled = enabled;

    if (!sensorCount || !ctx) {
       /**
        * always send Return OK,
        * Since, iio initialization takes some time.
        * Use activation_callback_info to set sampling frequency on initialization
        */
        devlist[handle].is_activate_pending = true;
        devlist[handle].activation_pending_state = enabled;
        return 0;
    }

    const char *sensor_name = id_map[handle].name;
    const struct iio_device *device = devlist[handle].dev;

    /* skip, if device not initialized*/
    if (!devlist[handle].is_initialized)
        return 0;

    active_sensor_count = 0;
    for (int id = 0; id < MAX_SENSOR; id++)
        if (devlist[id].is_initialized && devlist[id].is_enabled)
            active_sensor_count++;

    ALOGI("Device info ->  Sensor(%s): %s -> active_sensor_count(%d)",
            sensor_name, enabled?"enabled":"disabled", active_sensor_count);

    unsigned int raw_channel_count = devlist[handle].raw_channel_count;
    
    /* Activate or Deactivate all sensor */
    for (int index = 0; index < devlist[handle].nb_channels; index++) {
        struct iio_channel *channel = iio_device_get_channel(device, index);
        /* skip output channels */
        if (iio_channel_is_output(channel))
            continue;
 
        /* enable/disable input channels only */
        if(enabled)
            iio_channel_enable(channel);
        else
            iio_channel_disable(channel);

        ALOGI("%s channel(%d)",
                enabled?"Activated":"Deactivated", index);
    }

    return 0;
}

/**
 * Sets a sensor's parameters, including sampling frequency and maximum
 * report latency. This function can be called while the sensor is
 * activated.
 */
int iioClient::batch(int handle, int64_t sampling_period_ns, int64_t max_report_latency_ns) {
    if ((handle < 0) || (handle > MAX_SENSOR)) {
        ALOGE("Warning: batch invalid handle sampling_time(%lld) sensor hadle(%d) is out of range",
              (long long)sampling_period_ns, handle);
        return 0;
    }

    /* sampling period corrections */
    if (sampling_period_ns < kDefaultMinDelayUs * 1000) {
        sampling_period_ns = kDefaultMinDelayUs * 1000;
    } else if (sampling_period_ns > kDefaultMaxDelayUs * 1000) {
        sampling_period_ns = kDefaultMaxDelayUs * 1000;
    }

    /* Skip, if device not initialized */
    if (!devlist[handle].is_initialized || !sampling_period_ns)
        return 0;

    /* Convert time from ns to us and keep min time 1000us */
    int64_t sampling_period_us = sampling_period_ns/1000;
    devlist[handle].sampling_period_us = sampling_period_us;
    mSamplingPeriodUs = sampling_period_us;

    /**
     * Always send Return OK,
     * Since, iio initialization takes some time.
     * Use batch_pending status to set sampling frequency on initialization.
     */
    if (!sensorCount || !ctx) {
        devlist[handle].is_batch_pending = true;
        return 0;
    }

    const char *sensor_name = id_map[handle].name;
    struct iio_channel *channel = devlist[handle].channel_frequency;

    /* Calculate frequency from sampling time(ns) */
    double write_freq = (float) (1000000000/sampling_period_ns);
    if (iio_channel_attr_write_double(channel, "sampling_frequency", write_freq)) {
        ALOGD("Write error: batch -> Sensor(%s) sampling_period_ns(%lld) freq(%f)",
              sensor_name, (long long)sampling_period_ns, write_freq);
        return 0;
    }

    /* Read confirmation of frequency value */
    double read_freq = 0;
    if (iio_channel_attr_read_double(channel, "sampling_frequency", &read_freq)) {
        ALOGD("Read error: batch -> Sensor(%s) sampling_period_ns(%lld) freq(%f)",
              sensor_name, (long long)sampling_period_ns, read_freq);
        return 0;
    }

    ALOGD("Success: batch -> Sensor(%s), sampling_period_ns(%lld) max_report_latency_ns(%lld) freq(%f %f) max_events (%d) wait_time(%lld)",
           sensor_name, (long long)sampling_period_ns, (long long) max_report_latency_ns, write_freq, read_freq, (int) (max_report_latency_ns/sampling_period_ns),
           (long long) devlist[handle].sampling_period_us);

    return 0;
}
