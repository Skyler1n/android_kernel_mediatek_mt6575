/*
 * MStar MSG2133 touch panel driver for the MT6575 TPD framework.
 *
 * The Lenovo A60+ stock kernel exposes msg2133 at i2c-0/0-004c.
 * Board GPIO and power sequencing is supplied by the product header.
 */

#include "tpd.h"
#include "tpd_custom_msg2133.h"

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <cust_eint.h>
#include "cust_gpio_usage.h"

#ifdef MT6575
#include <mach/mt6575_boot.h>
#include <mach/mt6575_pm_ldo.h>
#include <mach/mt6575_typedefs.h>
#endif

#define MSG2133_I2C_ADDR        0x4c
#define MSG2133_I2C_NAME        "msg2133"
#define MSG2133_PACKET_SIZE     8
#define MSG2133_PACKET_ID       0x52
#define MSG2133_MAX_RAW         2048
#define MSG2133_MAX_POINTS      2
#define MSG2133_POLL_MS         20

#ifndef MSG2133_BOARD_POWER_ON
#error "MSG2133_BOARD_POWER_ON must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_POWER_OFF
#error "MSG2133_BOARD_POWER_OFF must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_RESET
#error "MSG2133_BOARD_RESET must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_PREPARE_EINT
#error "MSG2133_BOARD_PREPARE_EINT must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_EINT_NUM
#error "MSG2133_BOARD_EINT_NUM must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_EINT_SENSITIVE
#error "MSG2133_BOARD_EINT_SENSITIVE must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_EINT_DEBOUNCE_EN
#error "MSG2133_BOARD_EINT_DEBOUNCE_EN must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_EINT_DEBOUNCE_CN
#error "MSG2133_BOARD_EINT_DEBOUNCE_CN must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_EINT_POLARITY
#error "MSG2133_BOARD_EINT_POLARITY must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_MAP_X
#error "MSG2133_BOARD_MAP_X must be defined in tpd_custom_msg2133.h"
#endif

#ifndef MSG2133_BOARD_MAP_Y
#error "MSG2133_BOARD_MAP_Y must be defined in tpd_custom_msg2133.h"
#endif

extern struct tpd_device *tpd;
extern int tpd_type_cap;

struct msg2133_point {
    int x;
    int y;
};

static struct i2c_client *i2c_client;
static struct task_struct *thread;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag;
static int tpd_halt;

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

static const struct i2c_device_id msg2133_tpd_id[] = { { MSG2133_I2C_NAME, 0 }, { } };
static unsigned short force[] = { TPD_I2C_NUMBER, MSG2133_I2C_ADDR,
                                  I2C_CLIENT_END, I2C_CLIENT_END };
static const unsigned short * const forces[] = { force, NULL };
static struct i2c_client_address_data addr_data = { .forces = forces };

static void tpd_eint_interrupt_handler(void);
static int msg2133_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id);
static int msg2133_i2c_detect(struct i2c_client *client, int kind,
                              struct i2c_board_info *info);
static int msg2133_i2c_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);

extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity,
                                     void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);

static struct i2c_driver msg2133_i2c_driver = {
    .probe = msg2133_i2c_probe,
    .remove = msg2133_i2c_remove,
    .detect = msg2133_i2c_detect,
    .driver.name = MSG2133_I2C_NAME,
    .id_table = msg2133_tpd_id,
    .address_data = &addr_data,
};

static unsigned char msg2133_checksum(const unsigned char *packet)
{
    int i;
    int sum = 0;

    for (i = 0; i < MSG2133_PACKET_SIZE - 1; i++)
        sum += packet[i];

    return (unsigned char)((-sum) & 0xff);
}

static int msg2133_i2c_read(unsigned char *packet)
{
    int ret;
    static int read_fail_count;
    static int read_ok_logged;
    unsigned short old_addr = i2c_client->addr;

    i2c_client->timing = 100;
    i2c_client->addr = MSG2133_I2C_ADDR | I2C_ENEXT_FLAG;
    ret = i2c_master_recv(i2c_client, packet, MSG2133_PACKET_SIZE);
    i2c_client->addr = old_addr;

    if (ret != MSG2133_PACKET_SIZE) {
        read_fail_count++;
        if (read_fail_count <= 10 || (read_fail_count % 100) == 0)
            TPD_DMESG("msg2133 i2c read failed: %d, count=%d\n",
                      ret, read_fail_count);
        return ret < 0 ? ret : -EIO;
    }

    if (!read_ok_logged) {
        read_ok_logged = 1;
        TPD_DMESG("msg2133 first packet: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                  packet[0], packet[1], packet[2], packet[3],
                  packet[4], packet[5], packet[6], packet[7]);
    }

    return 0;
}

static void msg2133_power_on(void)
{
    MSG2133_BOARD_POWER_ON();
}

static void msg2133_power_off(void)
{
    MSG2133_BOARD_POWER_OFF();
}

static void msg2133_reset(void)
{
    MSG2133_BOARD_RESET();
    TPD_DMESG("msg2133 board reset done\n");
}

static int msg2133_parse_packet(unsigned char *packet,
                                struct msg2133_point *points)
{
    int x1;
    int y1;
    int dx;
    int dy;

    static int bad_packet_count;

    if (packet[0] != MSG2133_PACKET_ID)
        return -EINVAL;

    if (msg2133_checksum(packet) != packet[MSG2133_PACKET_SIZE - 1]) {
        bad_packet_count++;
        if (bad_packet_count <= 10 || (bad_packet_count % 100) == 0)
            TPD_DMESG("msg2133 checksum mismatch: got=%02x calc=%02x count=%d\n",
                      packet[MSG2133_PACKET_SIZE - 1],
                      msg2133_checksum(packet), bad_packet_count);
    }

    if (packet[1] == 0xff && packet[2] == 0xff && packet[3] == 0xff &&
        packet[4] == 0xff) {
#ifdef TPD_HAVE_BUTTON
        switch (packet[5]) {
        case 0x01:
            points[0].x = tpd_keys_dim_local[0][0];
            points[0].y = tpd_keys_dim_local[0][1];
            return 1;
#if TPD_KEY_COUNT >= 2
        case 0x02:
            points[0].x = tpd_keys_dim_local[1][0];
            points[0].y = tpd_keys_dim_local[1][1];
            return 1;
#endif
#if TPD_KEY_COUNT >= 3
        case 0x04:
            points[0].x = tpd_keys_dim_local[2][0];
            points[0].y = tpd_keys_dim_local[2][1];
            return 1;
#endif
#if TPD_KEY_COUNT >= 4
        case 0x08:
            points[0].x = tpd_keys_dim_local[3][0];
            points[0].y = tpd_keys_dim_local[3][1];
            return 1;
#endif
        default:
            break;
        }
#endif
        return 0;
    }

    x1 = ((packet[1] & 0xf0) << 4) | packet[2];
    y1 = ((packet[1] & 0x0f) << 8) | packet[3];
    dx = ((packet[4] & 0xf0) << 4) | packet[5];
    dy = ((packet[4] & 0x0f) << 8) | packet[6];

    if (x1 >= MSG2133_MAX_RAW || y1 >= MSG2133_MAX_RAW)
        return -EINVAL;

    points[0].x = MSG2133_BOARD_MAP_X(x1, y1);
    points[0].y = MSG2133_BOARD_MAP_Y(x1, y1);

    if (dx == 0 && dy == 0)
        return 1;

    if (dx & 0x0800)
        dx |= 0xfffff000;
    if (dy & 0x0800)
        dy |= 0xfffff000;

    x1 += dx;
    y1 += dy;
    if (x1 < 0 || x1 >= MSG2133_MAX_RAW || y1 < 0 || y1 >= MSG2133_MAX_RAW)
        return -EINVAL;

    points[1].x = MSG2133_BOARD_MAP_X(x1, y1);
    points[1].y = MSG2133_BOARD_MAP_Y(x1, y1);

    return MSG2133_MAX_POINTS;
}

static void tpd_down(int x, int y)
{
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_EM_PRINT(x, y, x, y, 1, 1);
}

static void tpd_up(void)
{
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    input_mt_sync(tpd->dev);
}

static int touch_event_handler(void *unused)
{
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    struct msg2133_point points[MSG2133_MAX_POINTS];
    unsigned char packet[MSG2133_PACKET_SIZE];
    int point_num;
    int i;
    int button_down = 0;
    int button_x = 0;
    int button_y = 0;
    int touch_down = 0;

    sched_setscheduler(current, SCHED_RR, &param);

    do {
        while (tpd_halt) {
            tpd_flag = 0;
            msleep(20);
        }
        wait_event_interruptible_timeout(waiter,
                                         tpd_flag != 0 || kthread_should_stop(),
                                         msecs_to_jiffies(MSG2133_POLL_MS));
        tpd_flag = 0;

        if (kthread_should_stop())
            break;

        if (msg2133_i2c_read(packet) < 0)
            goto out_unmask;

        point_num = msg2133_parse_packet(packet, points);
        if (point_num < 0)
            goto out_unmask;

        if (point_num == 0) {
            if (button_down) {
                button_down = 0;
                tpd_button(button_x, button_y, 0);
            } else if (touch_down) {
                touch_down = 0;
                tpd_up();
            } else {
                goto out_unmask;
            }
            input_sync(tpd->dev);
            goto out_unmask;
        }

#ifdef TPD_HAVE_BUTTON
        if (point_num == 1 && points[0].y > TPD_BUTTON_HEIGHT) {
            button_down = 1;
            button_x = points[0].x;
            button_y = points[0].y;
            tpd_button(button_x, button_y, 1);
            input_sync(tpd->dev);
            goto out_unmask;
        }
#endif

        for (i = 0; i < point_num; i++)
            tpd_down(points[i].x, points[i].y);
        touch_down = 1;
        input_sync(tpd->dev);

out_unmask:
        mt65xx_eint_unmask(MSG2133_BOARD_EINT_NUM);
    } while (!kthread_should_stop());

    return 0;
}

static int msg2133_i2c_detect(struct i2c_client *client, int kind,
                              struct i2c_board_info *info)
{
    strcpy(info->type, MSG2133_I2C_NAME);
    return 0;
}

static void tpd_eint_interrupt_handler(void)
{
    mt65xx_eint_mask(MSG2133_BOARD_EINT_NUM);
    tpd_flag = 1;
    wake_up_interruptible(&waiter);
}

static int msg2133_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id)
{
    int err = 0;

    i2c_client = client;
    i2c_client->timing = 100;

    msg2133_power_on();
    msg2133_reset();

    MSG2133_BOARD_PREPARE_EINT();

    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) {
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
        return err;
    }

    mt65xx_eint_set_sens(MSG2133_BOARD_EINT_NUM, MSG2133_BOARD_EINT_SENSITIVE);
    mt65xx_eint_set_hw_debounce(MSG2133_BOARD_EINT_NUM,
                                MSG2133_BOARD_EINT_DEBOUNCE_CN);
    mt65xx_eint_registration(MSG2133_BOARD_EINT_NUM,
                             MSG2133_BOARD_EINT_DEBOUNCE_EN,
                             MSG2133_BOARD_EINT_POLARITY,
                             tpd_eint_interrupt_handler, 1);
    mt65xx_eint_unmask(MSG2133_BOARD_EINT_NUM);

    TPD_DMESG("msg2133 eint: num=%d sens=%d debounce=%d polarity=%d\n",
              MSG2133_BOARD_EINT_NUM, MSG2133_BOARD_EINT_SENSITIVE,
              MSG2133_BOARD_EINT_DEBOUNCE_CN, MSG2133_BOARD_EINT_POLARITY);

    tpd_type_cap = 1;
    tpd_load_status = 1;
    TPD_DMESG("MSG2133 touch panel probe done\n");

    return 0;
}

static int msg2133_i2c_remove(struct i2c_client *client)
{
    return 0;
}

static int tpd_local_init(void)
{
    TPD_DMESG("MediaTek MSG2133 touch panel driver init\n");

    if (i2c_add_driver(&msg2133_i2c_driver) != 0) {
        TPD_DMESG("unable to add MSG2133 i2c driver\n");
        return -1;
    }

#ifdef TPD_HAVE_BUTTON
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);
#endif
    tpd_type_cap = 1;
    return 0;
}

static void tpd_suspend(struct early_suspend *h)
{
    tpd_halt = 1;
    mt65xx_eint_mask(MSG2133_BOARD_EINT_NUM);
    msg2133_power_off();
}

static void tpd_resume(struct early_suspend *h)
{
    msg2133_power_on();
    msg2133_reset();
    tpd_halt = 0;
    mt65xx_eint_unmask(MSG2133_BOARD_EINT_NUM);
}

static struct tpd_driver_t tpd_device_driver = {
    .tpd_device_name = "MSG2133",
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
};

static int __init tpd_driver_init(void)
{
    if (tpd_driver_add(&tpd_device_driver) < 0)
        TPD_DMESG("add MSG2133 driver failed\n");
    return 0;
}

static void __exit tpd_driver_exit(void)
{
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
