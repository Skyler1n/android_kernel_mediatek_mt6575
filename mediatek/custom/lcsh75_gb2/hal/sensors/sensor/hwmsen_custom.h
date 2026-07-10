/*
 * Copyright (C) 2008 The Android Open Source Project
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
 */
 
#ifndef __HWMSEN_CUSTOM_H__ 
#define __HWMSEN_CUSTOM_H__

#define MAX_NUM_SENSORS                 3

#define CUST_ACCELEROMETER_NAME         "BMA222E 3-axis Accelerometer"
#define CUST_ACCELEROMETER_VENDOR       "Bosch"
#define CUST_ACCELEROMETER_MAX_RANGE    32.0f
#define CUST_ACCELEROMETER_RESOLUTION   (4.0f/1024.0f)
#define CUST_ACCELEROMETER_POWER        (130.0f/1000.0f)

#define CUST_PROXIMITY_NAME             "TMD2771 Proximity Sensor"
#define CUST_PROXIMITY_VENDOR           "TAOS"
#define CUST_PROXIMITY_MAX_RANGE        1.0f
#define CUST_PROXIMITY_RESOLUTION       1.0f
#define CUST_PROXIMITY_POWER            0.13f

#define CUST_LIGHT_NAME                 "TMD2771 Light Sensor"
#define CUST_LIGHT_VENDOR               "TAOS"
#define CUST_LIGHT_MAX_RANGE            10240.0f
#define CUST_LIGHT_RESOLUTION           1.0f
#define CUST_LIGHT_POWER                0.13f

#endif
