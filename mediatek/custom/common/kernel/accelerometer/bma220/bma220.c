/*
 * BMA220 accelerometer driver for MT6575 / MTK hwmsen.
 *
 * A60+ official kernel exposes /sys/bus/i2c/drivers/BMA220 bound to
 * /sys/devices/platform/mt6575-i2c.0/i2c-0/0-0014.  The original tree was
 * configured for ADXL345, so Android's hwmsen HAL had no accelerometer
 * operator attached.  This driver follows the same MTK hwmsen interface used
 * by the existing adxl345/bma* drivers, but targets BMA220 at i2c0/0x14.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <asm/atomic.h>

#ifdef MT6575
#include <mach/mt6575_devs.h>
#include <mach/mt6575_typedefs.h>
#include <mach/mt6575_gpio.h>
#include <mach/mt6575_pm_ldo.h>
#endif

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#include <cust_bma220.h>
#include "bma220.h"

#ifdef MT6575
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#else
#define POWER_NONE_MACRO 0
#endif

typedef enum {
	BMA_TRC_FILTER  = 0x01,
	BMA_TRC_RAWDATA = 0x02,
	BMA_TRC_IOCTL   = 0x04,
	BMA_TRC_CALI    = 0x08,
	BMA_TRC_INFO    = 0x10,
} BMA_TRC;

struct scale_factor {
	u8 whole;
	u8 fraction;
};

struct data_resolution {
	struct scale_factor scalefactor;
	int sensitivity;
};

struct bma220_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[BMA220_AXES_NUM + 1];
	s16 data[BMA220_AXES_NUM + 1];
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_drv;
#endif
};

static int bma220_i2c_probe(struct i2c_client *client,
				 const struct i2c_device_id *id);
static int bma220_i2c_remove(struct i2c_client *client);
static int bma220_i2c_detect(struct i2c_client *client, int kind,
				  struct i2c_board_info *info);
static int bma220_suspend(struct i2c_client *client, pm_message_t msg);
static int bma220_resume(struct i2c_client *client);

static const struct i2c_device_id bma220_i2c_id[] = {
	{ BMA220_DEV_NAME, 0 },
	{ }
};

static unsigned short bma220_force[] = {
	0x00, BMA220_I2C_SLAVE_WRITE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END
};
static const unsigned short *const bma220_forces[] = { bma220_force, NULL };
static struct i2c_client_address_data bma220_addr_data = {
	.forces = bma220_forces,
};

static struct i2c_driver bma220_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = BMA220_DEV_NAME,
	},
	.probe = bma220_i2c_probe,
	.remove = bma220_i2c_remove,
	.detect = bma220_i2c_detect,
	.suspend = bma220_suspend,
	.resume = bma220_resume,
	.id_table = bma220_i2c_id,
	.address_data = &bma220_addr_data,
};

static struct i2c_client *bma220_i2c_client;
static struct platform_driver bma220_gsensor_driver;
static struct bma220_i2c_data *obj_i2c_data;
static bool sensor_power;
static GSENSOR_VECTOR3D gsensor_gain;

#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN()                printk(KERN_INFO GSE_TAG "%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG "%s %d : " fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)

static struct data_resolution bma220_data_resolution[] = {
	{{ 62, 3 }, BMA220_SENSITIVITY_2G},
};

static void BMA220_power(struct acc_hw *hw, unsigned int on)
{
	static unsigned int power_on;

	if (hw->power_id != POWER_NONE_MACRO) {
		GSE_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on) {
			GSE_LOG("ignore power control: %d\n", on);
		} else if (on) {
			if (!hwPowerOn(hw->power_id, hw->power_vol, "BMA220"))
				GSE_ERR("power on fails!!\n");
		} else {
			if (!hwPowerDown(hw->power_id, "BMA220"))
				GSE_ERR("power off fail!!\n");
		}
	}
	power_on = on;
}

static int BMA220_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[2];
	int res;

	databuf[0] = BMA220_REG_ID;
	res = i2c_master_send(client, databuf, 1);
	if (res <= 0)
		return BMA220_ERR_I2C;

	udelay(500);
	databuf[0] = 0;
	res = i2c_master_recv(client, databuf, 1);
	if (res <= 0)
		return BMA220_ERR_I2C;

	if (databuf[0] != BMA220_FIXED_DEVID) {
		GSE_ERR("unexpected chip id 0x%x\n", databuf[0]);
		return BMA220_ERR_IDENTIFICATION;
	}

	GSE_LOG("BMA220 chip id 0x%x\n", databuf[0]);
	return BMA220_SUCCESS;
}

static int BMA220_ReadToggleReg(struct i2c_client *client, u8 reg, u8 expect)
{
	u8 val;
	int i;
	int err;

	for (i = 0; i < 2; i++) {
		err = hwmsen_read_byte(client, reg, &val);
		if (err)
			return BMA220_ERR_I2C;
		if (val == expect)
			return BMA220_SUCCESS;
	}

	return BMA220_ERR_STATUS;
}

static int BMA220_SetPowerMode(struct i2c_client *client, bool enable)
{
	int err;

	if (enable == sensor_power)
		return BMA220_SUCCESS;

	/* BMA220 wake/suspend is controlled by reading this special register. */
	err = BMA220_ReadToggleReg(client, BMA220_REG_SUSPEND,
				 enable ? BMA220_SUSPEND_WAKE : BMA220_SUSPEND_SLEEP);
	if (err) {
		GSE_ERR("set power mode %d failed: %d\n", enable, err);
		return err;
	}

	sensor_power = enable;
	mdelay(5);
	return BMA220_SUCCESS;
}

static int BMA220_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 data;
	int err;

	err = hwmsen_read_byte(client, BMA220_REG_FILTER, &data);
	if (err)
		return BMA220_ERR_I2C;

	data &= ~BMA220_FILTER_MASK;
	data |= (bwrate & BMA220_FILTER_MASK);

	if (hwmsen_write_byte(client, BMA220_REG_FILTER, data))
		return BMA220_ERR_I2C;

	return BMA220_SUCCESS;
}

static int BMA220_SetDataFormat(struct i2c_client *client, u8 range)
{
	u8 data;
	int err;

	err = hwmsen_read_byte(client, BMA220_REG_RANGE, &data);
	if (err)
		return BMA220_ERR_I2C;

	data &= ~BMA220_RANGE_MASK;
	data |= (range & BMA220_RANGE_MASK);

	if (hwmsen_write_byte(client, BMA220_REG_RANGE, data))
		return BMA220_ERR_I2C;

	return BMA220_SUCCESS;
}

static int BMA220_ReadData(struct i2c_client *client, s16 data[BMA220_AXES_NUM])
{
	u8 raw[BMA220_AXES_NUM];
	int err;

	if (client == NULL)
		return -EINVAL;

	err = hwmsen_read_block(client, BMA220_REG_ACCEL_X, raw,
				       BMA220_AXES_NUM);
	if (err)
		return BMA220_ERR_I2C;
	data[BMA220_AXIS_X] = (s8)raw[BMA220_AXIS_X];
	data[BMA220_AXIS_Y] = (s8)raw[BMA220_AXIS_Y];
	data[BMA220_AXIS_Z] = (s8)raw[BMA220_AXIS_Z];

	if (obj_i2c_data) {
		data[BMA220_AXIS_X] += obj_i2c_data->cali_sw[BMA220_AXIS_X];
		data[BMA220_AXIS_Y] += obj_i2c_data->cali_sw[BMA220_AXIS_Y];
		data[BMA220_AXIS_Z] += obj_i2c_data->cali_sw[BMA220_AXIS_Z];
	}

	return BMA220_SUCCESS;
}

static int BMA220_ResetCalibration(struct i2c_client *client)
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return BMA220_SUCCESS;
}

static int BMA220_ReadCalibration(struct i2c_client *client,
				  int dat[BMA220_AXES_NUM])
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[BMA220_AXIS_X]] = obj->cvt.sign[BMA220_AXIS_X] *
		obj->cali_sw[BMA220_AXIS_X];
	dat[obj->cvt.map[BMA220_AXIS_Y]] = obj->cvt.sign[BMA220_AXIS_Y] *
		obj->cali_sw[BMA220_AXIS_Y];
	dat[obj->cvt.map[BMA220_AXIS_Z]] = obj->cvt.sign[BMA220_AXIS_Z] *
		obj->cali_sw[BMA220_AXIS_Z];

	return BMA220_SUCCESS;
}

static int BMA220_WriteCalibration(struct i2c_client *client,
				   int dat[BMA220_AXES_NUM])
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);

	obj->cali_sw[BMA220_AXIS_X] += obj->cvt.sign[BMA220_AXIS_X] *
		dat[obj->cvt.map[BMA220_AXIS_X]];
	obj->cali_sw[BMA220_AXIS_Y] += obj->cvt.sign[BMA220_AXIS_Y] *
		dat[obj->cvt.map[BMA220_AXIS_Y]];
	obj->cali_sw[BMA220_AXIS_Z] += obj->cvt.sign[BMA220_AXIS_Z] *
		dat[obj->cvt.map[BMA220_AXIS_Z]];

	return BMA220_SUCCESS;
}

static int bma220_init_client(struct i2c_client *client, int reset_cali)
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);
	int res;

	res = BMA220_CheckDeviceID(client);
	if (res != BMA220_SUCCESS)
		return res;

	res = BMA220_SetPowerMode(client, true);
	if (res != BMA220_SUCCESS)
		return res;

	/* Match upstream driver behaviour: enable short I2C watchdog. */
	hwmsen_write_byte(client, BMA220_REG_WDT, BMA220_WDT_1MS);

	res = BMA220_SetBWRate(client, BMA220_BW_125HZ);
	if (res != BMA220_SUCCESS)
		return res;

	res = BMA220_SetDataFormat(client, BMA220_RANGE_2G);
	if (res != BMA220_SUCCESS)
		return res;

	obj->reso = &bma220_data_resolution[0];
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

	if (reset_cali)
		BMA220_ResetCalibration(client);

	return BMA220_SUCCESS;
}

static int BMA220_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	if ((!buf) || (bufsize <= 30))
		return -EINVAL;

	snprintf(buf, bufsize, "BMA220 Chip");
	return 0;
}

static int BMA220_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct bma220_i2c_data *obj = obj_i2c_data;
	int acc[BMA220_AXES_NUM];
	int res;

	if ((!buf) || (bufsize <= 80))
		return -EINVAL;
	if (!client || !obj)
		return -EINVAL;

	if (!sensor_power) {
		res = BMA220_SetPowerMode(client, true);
		if (res)
			return res;
	}

	res = BMA220_ReadData(client, obj->data);
	if (res)
		return BMA220_ERR_GETGSENSORDATA;

	acc[obj->cvt.map[BMA220_AXIS_X]] = obj->cvt.sign[BMA220_AXIS_X] *
		obj->data[BMA220_AXIS_X];
	acc[obj->cvt.map[BMA220_AXIS_Y]] = obj->cvt.sign[BMA220_AXIS_Y] *
		obj->data[BMA220_AXIS_Y];
	acc[obj->cvt.map[BMA220_AXIS_Z]] = obj->cvt.sign[BMA220_AXIS_Z] *
		obj->data[BMA220_AXIS_Z];

	acc[BMA220_AXIS_X] = acc[BMA220_AXIS_X] * GRAVITY_EARTH_1000 /
		obj->reso->sensitivity;
	acc[BMA220_AXIS_Y] = acc[BMA220_AXIS_Y] * GRAVITY_EARTH_1000 /
		obj->reso->sensitivity;
	acc[BMA220_AXIS_Z] = acc[BMA220_AXIS_Z] * GRAVITY_EARTH_1000 /
		obj->reso->sensitivity;

	acc[BMA220_AXIS_X] = (acc[BMA220_AXIS_X] + CUST_BMA220_X_OFFSET_MG) *
		CUST_BMA220_X_SCALE_NUM / CUST_BMA220_X_SCALE_DEN;
	acc[BMA220_AXIS_Z] -= CUST_BMA220_Z_OFFSET_MG;

	snprintf(buf, bufsize, "%04x %04x %04x", acc[BMA220_AXIS_X],
		 acc[BMA220_AXIS_Y], acc[BMA220_AXIS_Z]);
	return 0;
}

static int BMA220_ReadRawData(struct i2c_client *client, char *buf)
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);
	int res;

	if (!buf || !client || !obj)
		return -EINVAL;

	res = BMA220_ReadData(client, obj->data);
	if (res)
		return res;

	sprintf(buf, "%04x %04x %04x", obj->data[BMA220_AXIS_X],
		obj->data[BMA220_AXIS_Y], obj->data[BMA220_AXIS_Z]);
	return 0;
}

int gsensor_operate(void *self, uint32_t command, void *buff_in, int size_in,
		    void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	int sample_delay;
	struct bma220_i2c_data *priv = (struct bma220_i2c_data *)self;
	hwm_sensor_data *gsensor_data;
	char buff[BMA220_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 5)
				sample_delay = BMA220_BW_250HZ;
			else if (value <= 10)
				sample_delay = BMA220_BW_125HZ;
			else if (value <= 20)
				sample_delay = BMA220_BW_64HZ;
			else
				sample_delay = BMA220_BW_32HZ;

			err = BMA220_SetBWRate(priv->client, sample_delay);
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			err = BMA220_SetPowerMode(priv->client, value ? true : false);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			GSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			gsensor_data = (hwm_sensor_data *)buff_out;
			err = BMA220_ReadSensorData(priv->client, buff, BMA220_BUFSIZE);
			if (!err) {
				sscanf(buff, "%x %x %x", &gsensor_data->values[0],
				       &gsensor_data->values[1], &gsensor_data->values[2]);
				gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				gsensor_data->value_divide = 1000;
			}
		}
		break;

	default:
		GSE_ERR("gsensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}

static int bma220_open(struct inode *inode, struct file *file)
{
	file->private_data = bma220_i2c_client;
	return nonseekable_open(inode, file);
}

static int bma220_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static int bma220_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);
	char strbuf[BMA220_BUFSIZE];
	void __user *data;
	SENSOR_DATA sensor_data;
	int err = 0;
	int cali[3];

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd),
			_IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		bma220_init_client(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		BMA220_ReadChipInfo(client, strbuf, BMA220_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1))
			err = -EFAULT;
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		BMA220_ReadSensorData(client, strbuf, BMA220_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1))
			err = -EFAULT;
		break;

	case GSENSOR_IOCTL_READ_GAIN:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_to_user(data, &gsensor_gain, sizeof(GSENSOR_VECTOR3D)))
			err = -EFAULT;
		break;

	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		BMA220_ReadRawData(client, strbuf);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1))
			err = -EFAULT;
		break;

	case GSENSOR_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			err = -EINVAL;
		} else {
			cali[BMA220_AXIS_X] = sensor_data.x * obj->reso->sensitivity /
				GRAVITY_EARTH_1000;
			cali[BMA220_AXIS_Y] = sensor_data.y * obj->reso->sensitivity /
				GRAVITY_EARTH_1000;
			cali[BMA220_AXIS_Z] = sensor_data.z * obj->reso->sensitivity /
				GRAVITY_EARTH_1000;
			err = BMA220_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = BMA220_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = BMA220_ReadCalibration(client, cali);
		if (err)
			break;
		sensor_data.x = cali[BMA220_AXIS_X] * GRAVITY_EARTH_1000 /
			obj->reso->sensitivity;
		sensor_data.y = cali[BMA220_AXIS_Y] * GRAVITY_EARTH_1000 /
			obj->reso->sensitivity;
		sensor_data.z = cali[BMA220_AXIS_Z] * GRAVITY_EARTH_1000 /
			obj->reso->sensitivity;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			err = -EFAULT;
		break;

	default:
		GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

static struct file_operations bma220_fops = {
	.owner = THIS_MODULE,
	.open = bma220_open,
	.release = bma220_release,
	.ioctl = bma220_ioctl,
};

static struct miscdevice bma220_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &bma220_fops,
};

static int bma220_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL)
			return -EINVAL;
		atomic_set(&obj->suspend, 1);
		err = BMA220_SetPowerMode(obj->client, false);
		BMA220_power(obj->hw, 0);
	}

	return err;
}

static int bma220_resume(struct i2c_client *client)
{
	struct bma220_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	if (obj == NULL)
		return -EINVAL;

	BMA220_power(obj->hw, 1);
	err = bma220_init_client(client, 0);
	if (err)
		return err;
	atomic_set(&obj->suspend, 0);
	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void bma220_early_suspend(struct early_suspend *h)
{
	struct bma220_i2c_data *obj = container_of(h, struct bma220_i2c_data,
					       early_drv);

	if (obj == NULL)
		return;
	atomic_set(&obj->suspend, 1);
	BMA220_SetPowerMode(obj->client, false);
	BMA220_power(obj->hw, 0);
}

static void bma220_late_resume(struct early_suspend *h)
{
	struct bma220_i2c_data *obj = container_of(h, struct bma220_i2c_data,
					       early_drv);

	if (obj == NULL)
		return;
	BMA220_power(obj->hw, 1);
	bma220_init_client(obj->client, 0);
	atomic_set(&obj->suspend, 0);
}
#endif

static int bma220_i2c_detect(struct i2c_client *client, int kind,
				  struct i2c_board_info *info)
{
	strcpy(info->type, BMA220_DEV_NAME);
	return 0;
}

static int bma220_i2c_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bma220_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;

	GSE_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	obj->hw = get_cust_acc_hw();
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}

	obj_i2c_data = obj;
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	atomic_set(&obj->filter, 0);

	bma220_i2c_client = client;
	err = bma220_init_client(client, 1);
	if (err)
		goto exit_kfree;

	err = misc_register(&bma220_device);
	if (err) {
		GSE_ERR("bma220_device register failed\n");
		goto exit_kfree;
	}

	sobj.self = obj;
	sobj.polling = 1;
	sobj.sensor_operate = gsensor_operate;
	err = hwmsen_attach(ID_ACCELEROMETER, &sobj);
	if (err) {
		GSE_ERR("attach fail = %d\n", err);
		goto exit_misc;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND)
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
	obj->early_drv.suspend = bma220_early_suspend;
	obj->early_drv.resume = bma220_late_resume;
	register_early_suspend(&obj->early_drv);
#endif

	GSE_LOG("%s: OK\n", __func__);
	return 0;

exit_misc:
	misc_deregister(&bma220_device);
exit_kfree:
	bma220_i2c_client = NULL;
	obj_i2c_data = NULL;
	kfree(obj);
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	return err;
}

static int bma220_i2c_remove(struct i2c_client *client)
{
	hwmsen_detach(ID_ACCELEROMETER);
	misc_deregister(&bma220_device);
	bma220_i2c_client = NULL;
	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static int bma220_probe(struct platform_device *pdev)
{
	struct acc_hw *hw = get_cust_acc_hw();

	GSE_FUN();
	BMA220_power(hw, 1);
	bma220_force[0] = hw->i2c_num;
	if (i2c_add_driver(&bma220_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}

static int bma220_remove(struct platform_device *pdev)
{
	struct acc_hw *hw = get_cust_acc_hw();

	GSE_FUN();
	BMA220_power(hw, 0);
	i2c_del_driver(&bma220_i2c_driver);
	return 0;
}

static struct platform_driver bma220_gsensor_driver = {
	.probe = bma220_probe,
	.remove = bma220_remove,
	.driver = {
		.name = "gsensor",
		.owner = THIS_MODULE,
	},
};

static int __init bma220_init(void)
{
	GSE_FUN();
	if (platform_driver_register(&bma220_gsensor_driver)) {
		GSE_ERR("failed to register driver\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit bma220_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&bma220_gsensor_driver);
}

module_init(bma220_init);
module_exit(bma220_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMA220 I2C driver");
MODULE_AUTHOR("OpenCode");
