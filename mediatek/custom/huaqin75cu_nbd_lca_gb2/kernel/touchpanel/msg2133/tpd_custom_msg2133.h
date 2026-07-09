#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

#define TPD_TYPE_CAPACITIVE
#define TPD_POWER_SOURCE        MT6575_POWER_VGP2
#define TPD_I2C_NUMBER          0
#define TPD_WAKEUP_TRIAL        60
#define TPD_WAKEUP_DELAY        100

#define TPD_HAVE_BUTTON
#define TPD_BUTTON_HEIGHT       480
#define TPD_KEY_COUNT           3

/* A60+ has three capacitive keys; MSG2133 reports bits 1, 2, 4. */
#define TPD_KEYS                { KEY_MENU, KEY_HOME, KEY_BACK }
#define TPD_KEYS_DIM            {{53, 520, 100, 80}, {160, 520, 100, 80}, {267, 520, 100, 80}}

#define TPD_HAVE_TREMBLE_ELIMINATION

#define MSG2133_BOARD_EN_PIN              125
#define MSG2133_BOARD_EN_PIN_M_GPIO       0

#define MSG2133_BOARD_POWER_ON()                                      \
    do {                                                             \
        hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");           \
        hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_3300, "TP");            \
    } while (0)

#define MSG2133_BOARD_POWER_OFF()                                     \
    do {                                                             \
        mt_set_gpio_mode(MSG2133_BOARD_EN_PIN,                       \
                         MSG2133_BOARD_EN_PIN_M_GPIO);               \
        mt_set_gpio_dir(MSG2133_BOARD_EN_PIN, GPIO_DIR_OUT);          \
        mt_set_gpio_out(MSG2133_BOARD_EN_PIN, GPIO_OUT_ZERO);         \
        mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);  \
        mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);             \
        mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_IN);               \
        mt_set_gpio_pull_enable(GPIO_CTP_RST_PIN, GPIO_PULL_ENABLE);  \
        mt_set_gpio_pull_select(GPIO_CTP_RST_PIN, GPIO_PULL_DOWN);    \
    } while (0)

#define MSG2133_BOARD_RESET()                                         \
    do {                                                             \
        MSG2133_BOARD_POWER_OFF();                                    \
        msleep(30);                                                   \
        mt_set_gpio_out(MSG2133_BOARD_EN_PIN, GPIO_OUT_ONE);          \
        msleep(200);                                                  \
    } while (0)

#define MSG2133_BOARD_PREPARE_EINT()                                  \
    do {                                                             \
        mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);\
        mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);              \
        mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE); \
        mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);     \
    } while (0)

#define MSG2133_BOARD_EINT_NUM          CUST_EINT_TOUCH_PANEL_NUM
#define MSG2133_BOARD_EINT_SENSITIVE    0
#define MSG2133_BOARD_EINT_DEBOUNCE_EN  0
#define MSG2133_BOARD_EINT_DEBOUNCE_CN  0
#define MSG2133_BOARD_EINT_POLARITY     1

#define MSG2133_BOARD_MAP_X(raw_x, raw_y) \
    ((TPD_RES_X - 1) - (((raw_y) * (TPD_RES_X - 1)) / 2047))
#define MSG2133_BOARD_MAP_Y(raw_x, raw_y) \
    (((raw_x) * (TPD_RES_Y - 1)) / 2047)

#endif /* TOUCHPANEL_H__ */
