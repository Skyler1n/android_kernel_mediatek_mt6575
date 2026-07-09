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

/* A690 has three capacitive keys; MSG2133 reports bits 1, 2, 4. */
#define TPD_KEYS                { KEY_MENU, KEY_HOME, KEY_BACK }
#define TPD_KEYS_DIM            {{80, 850, 160, 100}, {240, 850, 160, 100}, {400, 850, 160, 100}}

#define TPD_HAVE_TREMBLE_ELIMINATION

#endif /* TOUCHPANEL_H__ */
