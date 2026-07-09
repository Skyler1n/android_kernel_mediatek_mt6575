/*
 * BMA220 accelerometer definitions for MTK hwmsen integration.
 */
#ifndef BMA220_H
#define BMA220_H

#include <linux/ioctl.h>

/* MTK MT6575 i2c code and sysfs expose the 8-bit write address. */
#define BMA220_I2C_SLAVE_WRITE_ADDR    0x14

/* A60+ official BMA220 driver reads 8-bit X/Y/Z samples from 0x04..0x06. */
#define BMA220_REG_ID                  0x00
#define BMA220_REG_ACCEL_X             0x04
#define BMA220_REG_ACCEL_Y             0x05
#define BMA220_REG_ACCEL_Z             0x06
#define BMA220_REG_FILTER              0x20
#define BMA220_REG_RANGE               0x22
#define BMA220_REG_WDT                 0x2E
#define BMA220_REG_SUSPEND             0x30
#define BMA220_REG_SOFTRESET           0x32

#define BMA220_FIXED_DEVID             0xDD

#define BMA220_RANGE_MASK              0x03
#define BMA220_RANGE_2G                0x00
#define BMA220_RANGE_4G                0x01
#define BMA220_RANGE_8G                0x02
#define BMA220_RANGE_16G               0x03

#define BMA220_FILTER_MASK             0x0F
#define BMA220_BW_1000HZ               0x00
#define BMA220_BW_500HZ                0x01
#define BMA220_BW_250HZ                0x02
#define BMA220_BW_125HZ                0x03
#define BMA220_BW_64HZ                 0x04
#define BMA220_BW_32HZ                 0x05

#define BMA220_WDT_1MS                 0x04

#define BMA220_SUSPEND_WAKE            0xFF
#define BMA220_SUSPEND_SLEEP           0x00

#define BMA220_SUCCESS                 0
#define BMA220_ERR_I2C                 -1
#define BMA220_ERR_STATUS              -3
#define BMA220_ERR_SETUP_FAILURE       -4
#define BMA220_ERR_GETGSENSORDATA      -5
#define BMA220_ERR_IDENTIFICATION      -6

#define BMA220_BUFSIZE                 256

#define BMA220_AXIS_X                  0
#define BMA220_AXIS_Y                  1
#define BMA220_AXIS_Z                  2
#define BMA220_AXES_NUM                3
#define BMA220_DEV_NAME                "BMA220"

/* +/-2g, 8-bit signed data: 256 counts over 4g, 64 counts per g. */
#define BMA220_SENSITIVITY_2G          64

#endif
