#ifndef _CUST_LEDS_DEF_H
#define _CUST_LEDS_DEF_H

//#define CUST_LEDS_BACKLIGHT_PMIC_PARA /* parallel */
//#define CUST_LEDS_BACKLIGHT_PMIC_SERI /* series */

/* A60+: replacement LCD backlight uses GPIO68 as MT6575 PWM3_6x output. */
#define CUST_LEDS_LCD_BACKLIGHT_PWM_PINMUX()                         \
	do {                                                            \
		mt_set_gpio_dir(68, GPIO_DIR_OUT);                         \
		mt_set_gpio_pull_enable(68, GPIO_PULL_DISABLE);             \
		mt_set_gpio_out(68, GPIO_OUT_ONE);                          \
		mt_set_gpio_mode(68, GPIO_MODE_01);                         \
	} while (0)

#endif /* _CUST_LEDS_DEF_H */
