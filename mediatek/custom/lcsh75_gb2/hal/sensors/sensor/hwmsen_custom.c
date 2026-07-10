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
#include <hardware/sensors.h>
#include <linux/hwmsensor.h>
#include "hwmsen_custom.h"

struct sensor_t sSensorList[MAX_NUM_SENSORS] = 
{
	{  
		.name       = CUST_ACCELEROMETER_NAME,
		.vendor     = CUST_ACCELEROMETER_VENDOR,
		.version    = 1,
		.handle     = ID_ACCELEROMETER,
		.type       = SENSOR_TYPE_ACCELEROMETER,
		.maxRange   = CUST_ACCELEROMETER_MAX_RANGE,
		.resolution = CUST_ACCELEROMETER_RESOLUTION,
		.power      = CUST_ACCELEROMETER_POWER,
		.reserved   = {}
	},        

	{ 
		.name       = CUST_PROXIMITY_NAME,
		.vendor     = CUST_PROXIMITY_VENDOR,
		.version    = 1,
		.handle     = ID_PROXIMITY,
		.type       = SENSOR_TYPE_PROXIMITY,
		.maxRange   = CUST_PROXIMITY_MAX_RANGE,
		.resolution = CUST_PROXIMITY_RESOLUTION,
		.power      = CUST_PROXIMITY_POWER,
		.reserved   = {}
	},

	{ 
		.name       = CUST_LIGHT_NAME,
		.vendor     = CUST_LIGHT_VENDOR,
		.version    = 1,
		.handle     = ID_LIGHT,
		.type       = SENSOR_TYPE_LIGHT,
		.maxRange   = CUST_LIGHT_MAX_RANGE,
		.resolution = CUST_LIGHT_RESOLUTION,
		.power      = CUST_LIGHT_POWER,
		.reserved   = {}
	},
};
