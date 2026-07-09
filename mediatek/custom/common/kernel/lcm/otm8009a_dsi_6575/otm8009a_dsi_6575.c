#include <linux/string.h>

#include "lcm_drv.h"

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (800)

#define REGFLAG_DELAY              0xFE
#define REGFLAG_END_OF_TABLE       0xFFF
#define LCM_ID                     0x8009

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v) (lcm_util.set_reset_pin((v)))
#define MDELAY(n)        (lcm_util.mdelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
	unsigned cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
	{0x00, 1, {0x00}}, {0xFF, 3, {0x80, 0x09, 0x01}},
	{0x00, 1, {0x80}}, {0xFF, 2, {0x80, 0x09}},
	{0x00, 1, {0x03}}, {0xFF, 1, {0x01}},
	{0x00, 1, {0xB4}}, {0xC0, 1, {0x10}},
	{0x00, 1, {0x89}}, {0xC4, 1, {0x08}},
	{0x00, 1, {0xA3}}, {0xC0, 1, {0x00}},
	{0x00, 1, {0x82}}, {0xC5, 1, {0xA3}},
	{0x00, 1, {0x90}}, {0xC5, 2, {0xD6, 0x87}},
	{0x00, 1, {0x00}}, {0xD8, 2, {0x74, 0x72}},
	{0x00, 1, {0x00}}, {0xD9, 1, {0x50}},
	{0x00, 1, {0x00}}, {0xE1, 16, {0x09, 0x0C, 0x12, 0x0E,
						 0x08, 0x19, 0x0C, 0x0B,
						 0x01, 0x05, 0x03, 0x07,
						 0x0E, 0x26, 0x23, 0x1B}},
	{0x00, 1, {0x00}}, {0xE2, 16, {0x09, 0x0C, 0x12, 0x0E,
						 0x08, 0x19, 0x0C, 0x0B,
						 0x01, 0x05, 0x03, 0x07,
						 0x0E, 0x26, 0x23, 0x1B}},
	{0x00, 1, {0x81}}, {0xC1, 1, {0x66}},
	{0x00, 1, {0xA1}}, {0xC1, 1, {0x88}},
	{0x00, 1, {0x81}}, {0xC4, 1, {0x83}},
	{0x00, 1, {0x92}}, {0xC5, 1, {0x01}},
	{0x00, 1, {0xB1}}, {0xC5, 1, {0xA9}},
	{0x00, 1, {0x80}}, {0xCE, 12, {0x85, 0x03, 0x00, 0x84,
						 0x03, 0x00, 0x83, 0x03,
						 0x00, 0x82, 0x03, 0x00}},
	{0x00, 1, {0xA0}}, {0xCE, 14, {0x38, 0x02, 0x03, 0x21,
						 0x00, 0x00, 0x00, 0x38,
						 0x01, 0x03, 0x22, 0x00,
						 0x00, 0x00}},
	{0x00, 1, {0xB0}}, {0xCE, 14, {0x38, 0x00, 0x03, 0x23,
						 0x00, 0x00, 0x00, 0x30,
						 0x00, 0x03, 0x24, 0x00,
						 0x00, 0x00}},
	{0x00, 1, {0xC0}}, {0xCE, 14, {0x30, 0x01, 0x03, 0x25,
						 0x00, 0x00, 0x00, 0x30,
						 0x02, 0x03, 0x26, 0x00,
						 0x00, 0x00}},
	{0x00, 1, {0xD0}}, {0xCE, 14, {0x30, 0x03, 0x03, 0x27,
						 0x00, 0x00, 0x00, 0x30,
						 0x04, 0x03, 0x28, 0x00,
						 0x00, 0x00}},
	{0x00, 1, {0xC0}}, {0xCF, 10, {0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00}},
	{0x00, 1, {0xD0}}, {0xCF, 1, {0x00}},
	{0x00, 1, {0xC0}}, {0xCB, 15, {0x00, 0x00, 0x00, 0x00,
						 0x04, 0x04, 0x04, 0x04,
						 0x04, 0x04, 0x00, 0x00,
						 0x00, 0x00, 0x00}},
	{0x00, 1, {0xD0}}, {0xCB, 15, {0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x04, 0x04, 0x04,
						 0x04, 0x04, 0x04}},
	{0x00, 1, {0xE0}}, {0xCB, 10, {0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00}},
	{0x00, 1, {0x80}}, {0xCC, 10, {0x00, 0x00, 0x00, 0x00,
						 0x0C, 0x0A, 0x10, 0x0E,
						 0x03, 0x04}},
	{0x00, 1, {0x90}}, {0xCC, 15, {0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x0B}},
	{0x00, 1, {0xA0}}, {0xCC, 15, {0x09, 0x0F, 0x0D, 0x01,
						 0x02, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00}},
	{0x00, 1, {0xB0}}, {0xCC, 10, {0x00, 0x00, 0x00, 0x00,
						 0x0D, 0x0F, 0x09, 0x0B,
						 0x02, 0x01}},
	{0x00, 1, {0xC0}}, {0xCC, 15, {0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x0E}},
	{0x00, 1, {0xD0}}, {0xCC, 15, {0x10, 0x0A, 0x0C, 0x04,
						 0x03, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x00}},
	{0x00, 1, {0x00}}, {0xFF, 3, {0xFF, 0xFF, 0xFF}},
	{0x3A, 1, {0x77}},
	{0x36, 1, {0x00}},
	{0x35, 1, {0x00}},
	{0x11, 0, {}},
	{REGFLAG_DELAY, 50, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 200, {}},
	{0x2C, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
	{0x28, 0, {}},
	{0x10, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_sleep_out_setting[] = {
	{0x11, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_backlight_level_setting[] = {
	{0x51, 1, {0xFF}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
			       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			return;
		default:
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list,
					 force_update);
			break;
		}
	}
}

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->dbi.te_mode = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity = LCM_POLARITY_RISING;

	params->dsi.mode = CMD_MODE;
	params->dsi.LANE_NUM = LCM_TWO_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;
	params->dsi.packet_size = 256;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.word_count = FRAME_WIDTH * 3;
	params->dsi.vertical_sync_active = 2;
	params->dsi.vertical_backporch = 2;
	params->dsi.vertical_frontporch = 2;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.line_byte = 2180;
	params->dsi.horizontal_sync_active_byte = 26;
	params->dsi.horizontal_backporch_byte = 206;
	params->dsi.horizontal_frontporch_byte = 206;
	params->dsi.rgb_byte = FRAME_WIDTH * 3 + 6;

	params->dsi.horizontal_sync_active_word_count = 20;
	params->dsi.horizontal_backporch_word_count = 200;
	params->dsi.horizontal_frontporch_word_count = 200;

	params->dsi.pll_div1 = 38;
	params->dsi.pll_div2 = 1;
}

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);

	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) /
		   sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_deep_sleep_mode_in_setting,
		   sizeof(lcm_deep_sleep_mode_in_setting) /
		   sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
	push_table(lcm_sleep_out_setting,
		   sizeof(lcm_sleep_out_setting) /
		   sizeof(struct LCM_setting_table), 1);
}

static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;
	unsigned char x0_msb = ((x0 >> 8) & 0xFF);
	unsigned char x0_lsb = (x0 & 0xFF);
	unsigned char x1_msb = ((x1 >> 8) & 0xFF);
	unsigned char x1_lsb = (x1 & 0xFF);
	unsigned char y0_msb = ((y0 >> 8) & 0xFF);
	unsigned char y0_lsb = (y0 & 0xFF);
	unsigned char y1_msb = ((y1 >> 8) & 0xFF);
	unsigned char y1_lsb = (y1 & 0xFF);
	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_msb << 24) | (x0_lsb << 16) |
			(x0_msb << 8) | 0x2A;
	data_array[2] = x1_lsb;
	data_array[3] = 0x00053902;
	data_array[4] = (y1_msb << 24) | (y0_lsb << 16) |
			(y0_msb << 8) | 0x2B;
	data_array[5] = y1_lsb;
	data_array[6] = 0x002C3909;

	dsi_set_cmdq(data_array, 7, 0);
}

static void lcm_setbacklight(unsigned int level)
{
	if (level > 255)
		level = 255;

	lcm_backlight_level_setting[0].para_list[0] = level;
	push_table(lcm_backlight_level_setting,
		   sizeof(lcm_backlight_level_setting) /
		   sizeof(struct LCM_setting_table), 1);
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id;
	unsigned char buffer[2] = {0};
	unsigned int array[16];

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(25);
	SET_RESET_PIN(1);
	MDELAY(50);

	array[0] = 0x00043902;
	array[1] = 0x010980FF;
	array[2] = 0x80001500;
	array[3] = 0x00033902;
	array[4] = 0x010980FF;
	dsi_set_cmdq(array, 5, 1);
	MDELAY(10);

	array[0] = 0x00023700;
	dsi_set_cmdq(array, 1, 1);
	array[0] = 0x02001500;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xD2, buffer, 2);
	id = (buffer[0] << 8) | buffer[1];

	return (id == LCM_ID) ? 1 : 0;
}

LCM_DRIVER otm8009a_dsi_6575_lcm_drv = {
	.name = "otm8009a_dsi_6575",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.update = lcm_update,
	.set_backlight = lcm_setbacklight,
	.compare_id = lcm_compare_id,
};
