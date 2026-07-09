#ifndef TOUCHPANEL_H__
#define TOUCHPANEL_H__

#define TPD_TYPE_CAPACITIVE
#define TPD_POWER_SOURCE        MT6575_POWER_VGP2
#define TPD_I2C_NUMBER          0
#define TPD_WAKEUP_TRIAL        60
#define TPD_WAKEUP_DELAY        100

#define TPD_HAVE_BUTTON
#define TPD_BUTTON_HEIGHT       800
#define TPD_KEY_COUNT           3

/* A690 stock key table maps MSG2133 bits 1, 2, 4 to Back, Home, Menu. */
#define TPD_KEYS                { KEY_BACK, KEY_HOME, KEY_MENU }
#define TPD_KEYS_DIM            {{64, 867, 80, 80}, {181, 867, 80, 80}, {340, 867, 80, 80}}

#define TPD_HAVE_TREMBLE_ELIMINATION

/* A690 stock MSG2133 probe drives GPIO102 high, pulses GPIO86, and uses EINT6. */
#define MSG2133_BOARD_ENABLE_PIN          102
#define MSG2133_BOARD_RESET_PIN           86
#define MSG2133_BOARD_EINT_PIN            75
#define MSG2133_BOARD_GPIO_M_GPIO         0
#define MSG2133_BOARD_EINT_PIN_M_EINT     1

#define MSG2133_BOARD_POWER_ON()                                      \
    do {                                                             \
        mt_set_gpio_mode(MSG2133_BOARD_ENABLE_PIN,                   \
                         MSG2133_BOARD_GPIO_M_GPIO);                 \
        mt_set_gpio_dir(MSG2133_BOARD_ENABLE_PIN, GPIO_DIR_OUT);      \
        mt_set_gpio_out(MSG2133_BOARD_ENABLE_PIN, GPIO_OUT_ONE);      \
        msleep(50);                                                   \
        mt_set_gpio_mode(MSG2133_BOARD_RESET_PIN,                     \
                         MSG2133_BOARD_GPIO_M_GPIO);                 \
        mt_set_gpio_dir(MSG2133_BOARD_RESET_PIN, GPIO_DIR_OUT);       \
        mt_set_gpio_out(MSG2133_BOARD_RESET_PIN, GPIO_OUT_ONE);       \
        msleep(10);                                                   \
        mt_set_gpio_mode(MSG2133_BOARD_RESET_PIN,                     \
                         MSG2133_BOARD_GPIO_M_GPIO);                 \
        mt_set_gpio_dir(MSG2133_BOARD_RESET_PIN, GPIO_DIR_OUT);       \
        mt_set_gpio_out(MSG2133_BOARD_RESET_PIN, GPIO_OUT_ZERO);      \
        msleep(10);                                                   \
        mt_set_gpio_mode(MSG2133_BOARD_ENABLE_PIN,                   \
                         MSG2133_BOARD_GPIO_M_GPIO);                 \
        mt_set_gpio_dir(MSG2133_BOARD_ENABLE_PIN, GPIO_DIR_OUT);      \
        mt_set_gpio_out(MSG2133_BOARD_ENABLE_PIN, GPIO_OUT_ONE);      \
        hwPowerDown(MT65XX_POWER_LDO_VGP, "TP");                    \
        hwPowerDown(MT65XX_POWER_LDO_VCAM_AF, "TP");                \
        msleep(100);                                                  \
        msleep(30);                                                   \
        hwPowerOn(MT65XX_POWER_LDO_VGP, VOL_1800, "TP");            \
        hwPowerOn(MT65XX_POWER_LDO_VCAM_AF, VOL_2800, "TP");        \
        msleep(100);                                                  \
        msleep(30);                                                   \
    } while (0)

/* Stock A690 does not reset or power-cycle MSG2133 during suspend/resume. */
#define MSG2133_BOARD_POWER_OFF()                                     \
    do {                                                             \
    } while (0)

#define MSG2133_BOARD_RESUME()                                        \
    do {                                                             \
    } while (0)

#define MSG2133_BOARD_RESET()                                         \
    do {                                                             \
        mt_set_gpio_mode(MSG2133_BOARD_RESET_PIN,                     \
                         MSG2133_BOARD_GPIO_M_GPIO);                 \
        mt_set_gpio_dir(MSG2133_BOARD_RESET_PIN, GPIO_DIR_OUT);       \
        msleep(10);                                                   \
        mt_set_gpio_out(MSG2133_BOARD_RESET_PIN, GPIO_OUT_ZERO);      \
        msleep(50);                                                   \
    } while (0)

#define MSG2133_BOARD_PREPARE_EINT()                                  \
    do {                                                             \
        mt_set_gpio_mode(MSG2133_BOARD_EINT_PIN,                      \
                         MSG2133_BOARD_EINT_PIN_M_EINT);             \
        mt_set_gpio_dir(MSG2133_BOARD_EINT_PIN, GPIO_DIR_IN);         \
        mt_set_gpio_pull_enable(MSG2133_BOARD_EINT_PIN,               \
                                GPIO_PULL_ENABLE);                   \
        mt_set_gpio_pull_select(MSG2133_BOARD_EINT_PIN, GPIO_PULL_UP);\
        msleep(200);                                                  \
    } while (0)

#define MSG2133_BOARD_EINT_NUM          6
#define MSG2133_BOARD_EINT_SENSITIVE    1
#define MSG2133_BOARD_EINT_DEBOUNCE_EN  0
#define MSG2133_BOARD_EINT_DEBOUNCE_CN  0
#define MSG2133_BOARD_EINT_POLARITY     1

#define MSG2133_BOARD_MAP_X(raw_x, raw_y) \
    (((raw_y) * (TPD_RES_X - 1)) / 2047)
#define MSG2133_BOARD_MAP_Y(raw_x, raw_y) \
    (((raw_x) * (TPD_RES_Y - 1)) / 2047)

#endif /* TOUCHPANEL_H__ */
