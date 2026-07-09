#include <linux/types.h>
#include <cust_acc.h>
#include <mach/mt6575_pm_ldo.h>

static struct acc_hw cust_acc_hw = {
	.i2c_num = 0,
	.direction = 0,
	.power_id = MT65XX_POWER_NONE,
	.power_vol = VOL_DEFAULT,
	.firlen = 5,
};

struct acc_hw *get_cust_acc_hw(void)
{
	return &cust_acc_hw;
}
