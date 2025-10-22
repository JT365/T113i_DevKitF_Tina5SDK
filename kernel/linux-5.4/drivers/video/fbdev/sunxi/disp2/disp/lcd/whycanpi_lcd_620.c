/* drivers/video/sunxi/disp2/disp/lcd/tft08006.c
 *
 * Copyright (c) 2021 Allwinnertech Co., Ltd.
 *
 * tft08006 panel driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 &lcd0 {
	lcd_used            = <1>;

	lcd_driver_name     = "tft08006";
	lcd_backlight       = <50>;
	lcd_if              = <4>;

	lcd_x               = <800>;
	lcd_y               = <1280>;
	lcd_width           = <52>;
	lcd_height          = <52>;
	lcd_dclk_freq       = <68>;

	lcd_pwm_used        = <1>;
	lcd_pwm_ch          = <2>;
	lcd_pwm_freq        = <50000>;
	lcd_pwm_pol         = <1>;
	lcd_pwm_max_limit   = <255>;

	lcd_hbp             = <40>;
	lcd_ht              = <860>;
	lcd_hspw            = <20>;
	lcd_vbp             = <24>;
	lcd_vt              = <1330>;
	lcd_vspw            = <4>;

	lcd_dsi_if          = <0>;
	lcd_dsi_lane        = <4>;
	lcd_lvds_if         = <0>;
	lcd_lvds_colordepth = <0>;
	lcd_lvds_mode       = <0>;
	lcd_frm             = <0>;
	lcd_hv_clk_phase    = <0>;
	lcd_hv_sync_polarity= <0>;
	lcd_gamma_en        = <0>;
	lcd_bright_curve_en = <0>;
	lcd_cmap_en         = <0>;
	lcd_fsync_en        = <0>;
	lcd_fsync_act_time  = <1000>;
	lcd_fsync_dis_time  = <1000>;
	lcd_fsync_pol       = <0>;

	deu_mode            = <0>;
	lcdgamma4iep        = <22>;
	smart_color         = <90>;

	lcd_gpio_0 =  <&pio PG 13 GPIO_ACTIVE_HIGH>;
	pinctrl-0 = <&dsi4lane_pins_a>;
	pinctrl-1 = <&dsi4lane_pins_b>;
};
*/
#include "whycanpi_lcd_620.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define power_en(sel, val) sunxi_lcd_gpio_set_value(sel, 1, val)

static void lcd_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
			{LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
			{LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
			{LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
		},
		{
			{LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
			{LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
			{LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
		},
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i + 1][1] - lcd_gamma_tbl[i][1])
				 * j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
				(value << 16)
				+ (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8)
				   + lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 lcd_open_flow(u32 sel)
{
	LCD_OPEN_FUNC(sel, lcd_power_on, 10);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 10);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 50);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);

	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 200);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 500);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	power_en(sel, 1);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(100);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(20);
	sunxi_lcd_pin_cfg(sel, 1);

}

static void lcd_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(20);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(5);
	power_en(sel, 0);
}

static void lcd_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
	sunxi_lcd_pwm_disable(sel);
}


static void lcd_panel_init(u32 sel)
{
		
	//u8 comid = 0x23;
	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(100);
	
	sunxi_lcd_dsi_gen_write_1para(sel, 0xE0,0x00);

	sunxi_lcd_dsi_gen_write_1para(sel, 0xE1,0x93);
	sunxi_lcd_dsi_gen_write_1para(sel, 0xE2,0x65);
	sunxi_lcd_dsi_gen_write_1para(sel, 0xE3,0xF8);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x80,0x03);


	sunxi_lcd_dsi_gen_write_1para(sel, 0xE0,0x01);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x00,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x01,0x5B);  //0xA0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x03,0x10);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x04,0x37);  //0xA0

	sunxi_lcd_dsi_gen_write_1para(sel, 0x0C,0x74);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x17,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x18,0xDF);  //VGMP=4.9
	sunxi_lcd_dsi_gen_write_1para(sel, 0x19,0x01);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1A,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1B,0xDF);  //VGMN=-4.9
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1C,0x01);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x1F,0x2F);     //VGH_R  = 15V ,0X72   888
	sunxi_lcd_dsi_gen_write_1para(sel, 0x20,0x2F);     //VGL_R  = -12V
	sunxi_lcd_dsi_gen_write_1para(sel, 0x21,0x2F);     //VGL_R2 = -12V
	sunxi_lcd_dsi_gen_write_1para(sel, 0x22,0x0E);     //PA[6]=0, PA[5]=0, PA[4]=0, PA[0]=0

	sunxi_lcd_dsi_gen_write_1para(sel, 0x24,0xFE);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x37,0x09);	//SS=1,BGR=1

	sunxi_lcd_dsi_gen_write_1para(sel, 0x38,0x04);	//JDT=101 zigzag inversion
	sunxi_lcd_dsi_gen_write_1para(sel, 0x39,0x00);	//RGB_N_EQ1, modify 20140806
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3A,0x01);	//RGB_N_EQ2, modify 20140806
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3C,0x90);	//SET EQ3 for TE_H
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3D,0xFF);	//SET CHGEN_ON, modify 20140827
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3E,0xFF);	//SET CHGEN_OFF, modify 20140827
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3F,0xFF);	//SET CHGEN_OFF2, modify 20140827		7F


	sunxi_lcd_dsi_gen_write_1para(sel, 0x40,0x02);	//RSO=720 RGB
	sunxi_lcd_dsi_gen_write_1para(sel, 0x41,0xB2);	//LN=712->1424 line
	sunxi_lcd_dsi_gen_write_1para(sel, 0x43,0x06);	//VFP
	sunxi_lcd_dsi_gen_write_1para(sel, 0x44,0x0A);	//VBP
	sunxi_lcd_dsi_gen_write_1para(sel, 0x45,0x3C);	//HBP
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4B,0x04);  //source EQ off
	sunxi_lcd_dsi_gen_write_1para(sel, 0x55,0x02);	//02=2power mode 0C=3power mode
	sunxi_lcd_dsi_gen_write_1para(sel, 0x56,0x01);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x57,0x89);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x58,0x0A);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x59,0x0A);	//VCL = -2.9V
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5A,0x28);	//VGH = 15.2V
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5B,0x1A);	//VGL = -12.2V

	sunxi_lcd_dsi_gen_write_1para(sel, 0x5D,0x7C);     //0x7C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5E,0x57);     //0x53
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5F,0x44);     //0x40
	sunxi_lcd_dsi_gen_write_1para(sel, 0x60,0x36);     //0x31
	sunxi_lcd_dsi_gen_write_1para(sel, 0x61,0x31);     //0x2C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x62,0x23);     //0x1C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x63,0x26);     //0x1F
	sunxi_lcd_dsi_gen_write_1para(sel, 0x64,0x0F);     //0x08
	sunxi_lcd_dsi_gen_write_1para(sel, 0x65,0x28);     //0x21
	sunxi_lcd_dsi_gen_write_1para(sel, 0x66,0x26);     //0x1F
	sunxi_lcd_dsi_gen_write_1para(sel, 0x67,0x27);     //0x1F
	sunxi_lcd_dsi_gen_write_1para(sel, 0x68,0x45);     //0x3C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x69,0x35);     //0x2A
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6A,0x3D);     //0x30
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6B,0x2F);     //0x22
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6C,0x2B);     //0x1E
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6D,0x1E);     //0x12
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6E,0x0D);     //0x06
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6F,0x00);     //0x00
	sunxi_lcd_dsi_gen_write_1para(sel, 0x70,0x7C);     //0x7C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x71,0x57);     //0x53
	sunxi_lcd_dsi_gen_write_1para(sel, 0x72,0x44);     //0x40
	sunxi_lcd_dsi_gen_write_1para(sel, 0x73,0x36);     //0x31
	sunxi_lcd_dsi_gen_write_1para(sel, 0x74,0x31);     //0x2C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x75,0x23);     //0x1C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x76,0x26);     //0x1F
	sunxi_lcd_dsi_gen_write_1para(sel, 0x77,0x0F);     //0x08
	sunxi_lcd_dsi_gen_write_1para(sel, 0x78,0x28);     //0x21
	sunxi_lcd_dsi_gen_write_1para(sel, 0x79,0x26);     //0x1F
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7A,0x27);     //0x1F
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7B,0x45);     //0x3C
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7C,0x35);     //0x2A
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7D,0x3D);     //0x30
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7E,0x2F);     //0x22
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7F,0x2B);     //0x1E
	sunxi_lcd_dsi_gen_write_1para(sel, 0x80,0x1E);     //0x12
	sunxi_lcd_dsi_gen_write_1para(sel, 0x81,0x0D);     //0x06
	sunxi_lcd_dsi_gen_write_1para(sel, 0x82,0x00);     //0x00


	sunxi_lcd_dsi_gen_write_1para(sel, 0xE0,0x02);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x00,0x75); //GCL
	sunxi_lcd_dsi_gen_write_1para(sel, 0x01,0x50); //STV0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x02,0x55); //GCH
	sunxi_lcd_dsi_gen_write_1para(sel, 0x03,0x43); //STV3
	sunxi_lcd_dsi_gen_write_1para(sel, 0x04,0x5E); //VDS
	sunxi_lcd_dsi_gen_write_1para(sel, 0x05,0x5F); //VSD
	sunxi_lcd_dsi_gen_write_1para(sel, 0x06,0x41); //STV1
	sunxi_lcd_dsi_gen_write_1para(sel, 0x07,0x5F); //VGL
	sunxi_lcd_dsi_gen_write_1para(sel, 0x08,0x45); //CLK5
	sunxi_lcd_dsi_gen_write_1para(sel, 0x09,0x47); //CLK7
	sunxi_lcd_dsi_gen_write_1para(sel, 0x0A,0x49); //CLK1
	sunxi_lcd_dsi_gen_write_1para(sel, 0x0B,0x4B); //CLK3
	sunxi_lcd_dsi_gen_write_1para(sel, 0x0C,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x0D,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x0E,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x0F,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x10,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x11,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x12,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x13,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x14,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x15,0x5F); //

	sunxi_lcd_dsi_gen_write_1para(sel, 0x16,0x75); //GCL
	sunxi_lcd_dsi_gen_write_1para(sel, 0x17,0x50); //STV0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x18,0x55); //GCH
	sunxi_lcd_dsi_gen_write_1para(sel, 0x19,0x42); //STV4
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1A,0x5E); //VDS
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1B,0x5F); //VSD
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1C,0x40); //STV2
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1D,0x5F); //VGL
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1E,0x44); //CLK6
	sunxi_lcd_dsi_gen_write_1para(sel, 0x1F,0x46); //CLK8
	sunxi_lcd_dsi_gen_write_1para(sel, 0x20,0x48); //CLK2
	sunxi_lcd_dsi_gen_write_1para(sel, 0x21,0x4A); //CLK4
	sunxi_lcd_dsi_gen_write_1para(sel, 0x22,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x23,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x24,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x25,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x26,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x27,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x28,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x29,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x2A,0x5F); //
	sunxi_lcd_dsi_gen_write_1para(sel, 0x2B,0x5F); //

	sunxi_lcd_dsi_gen_write_1para(sel, 0x2C,0x5E);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x2D,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x2E,0x75);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x2F,0x50);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x30,0x47);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x31,0x47);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x32,0x45);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x33,0x45);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x34,0x4B);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x35,0x4B);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x36,0x49);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x37,0x49);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x38,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x39,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3A,0x55);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3B,0x55);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3C,0x43);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3D,0x41);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3E,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x3F,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x40,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x41,0x5F);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x42,0x5E);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x43,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x44,0x75);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x45,0x50);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x46,0x46);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x47,0x46);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x48,0x44);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x49,0x44);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4A,0x4A);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4B,0x4A);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4C,0x48);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4D,0x48);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4E,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x4F,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x50,0x55);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x51,0x55);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x52,0x42);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x53,0x40);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x54,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x55,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x56,0x5F);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x57,0x5F);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x58,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x59,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5A,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5B,0x30);	//STV_Num
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5C,0x00);	//STV_S0          888
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5D,0x30);	//STV_W,STV_S1
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5E,0x00);	//STV_S2
	sunxi_lcd_dsi_gen_write_1para(sel, 0x5F,0x00);	//STV_S3
	sunxi_lcd_dsi_gen_write_1para(sel, 0x60,0x30);	//ETV_W,ETV_S1
	sunxi_lcd_dsi_gen_write_1para(sel, 0x61,0x00);	//ETV_S2
	sunxi_lcd_dsi_gen_write_1para(sel, 0x62,0x00);	//ETV_S3
	sunxi_lcd_dsi_gen_write_1para(sel, 0x63,0x06);	//SETV_ON
	sunxi_lcd_dsi_gen_write_1para(sel, 0x64,0x6A);	//SETV_OFF
	sunxi_lcd_dsi_gen_write_1para(sel, 0x65,0x45);	//ETV_EN,ETV_NUM
	sunxi_lcd_dsi_gen_write_1para(sel, 0x66,0xAF);	//ETV_S0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x67,0x73);	//CKV0_NUM,CKV0_W
	sunxi_lcd_dsi_gen_write_1para(sel, 0x68,0x04);	//CKV0_S0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x69,0x06);	//CKV0_ON
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6A,0x6A);	//CKV0_OFF
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6B,0x08);	//CKV0_DUM        888

	sunxi_lcd_dsi_gen_write_1para(sel, 0x6C,0x00);	//EOLR,GEQ_LINE,GEQ_W
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6D,0x04);	//GEQ_GGND1
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6E,0x04);	//GEQ_GGND2
	sunxi_lcd_dsi_gen_write_1para(sel, 0x6F,0x88);	//GIPDR,VGHO_SEL,VGLO_SEL
	sunxi_lcd_dsi_gen_write_1para(sel, 0x70,0x00);	//CKV1_NUM,CKV1_W
	sunxi_lcd_dsi_gen_write_1para(sel, 0x71,0x00);	//CKV1_S0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x72,0x06);	//CKV1_ON
	sunxi_lcd_dsi_gen_write_1para(sel, 0x73,0x7B);	//CKV1_OFF
	sunxi_lcd_dsi_gen_write_1para(sel, 0x74,0x00);	//CKV1_DUM
	sunxi_lcd_dsi_gen_write_1para(sel, 0x75,0x07);	//FLM_EN,FLM_W    888
	sunxi_lcd_dsi_gen_write_1para(sel, 0x76,0x00);	//FLM_ON
	sunxi_lcd_dsi_gen_write_1para(sel, 0x77,0xD0);	//VEN_EN,VEN_W,FLM_NUM
	sunxi_lcd_dsi_gen_write_1para(sel, 0x78,0x17);	//FLM_OFF
	sunxi_lcd_dsi_gen_write_1para(sel, 0x79,0xB0);	//VEN_W
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7A,0x00);	//VEN_S0
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7B,0x00);	//VEN_S1
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7C,0x00);	//VEN_DUM
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7D,0x06);	//VEN_ON
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7E,0x6A);	//VEN_OFF
	sunxi_lcd_dsi_gen_write_1para(sel, 0x7F,0x40);



	sunxi_lcd_dsi_gen_write_1para(sel, 0xE0,0x00);
	sunxi_lcd_dsi_gen_write_1para(sel, 0xE6,0x02);
	sunxi_lcd_dsi_gen_write_1para(sel, 0xE7,0x0C);
	sunxi_lcd_dsi_gen_write_1para(sel, 0x35,0x00);
	sunxi_lcd_dsi_gen_write_0para(sel, 0x11);  	// SLPOUT
	sunxi_lcd_delay_ms(120);



	sunxi_lcd_dsi_gen_write_0para(sel, 0x29);  	// DSPON
	sunxi_lcd_delay_ms(5);

	sunxi_lcd_dsi_gen_write_1para(sel, 0x35,0x00);	
}

static void lcd_panel_exit(u32 sel)
{
	//sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	sunxi_lcd_delay_ms(80);
	//sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(50);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

struct __lcd_panel whycanpi_lcd_620_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "whycanpi_lcd_620",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
		.cfg_open_flow = lcd_open_flow,
		.cfg_close_flow = lcd_close_flow,
		.lcd_user_defined_func = lcd_user_defined_func,
	},
};
