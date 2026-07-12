/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.c
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Source code of Sensor driver
 *
 *
 * Author:
 * -------
 *   Anyuan Huang (MTK70663)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/system.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "hi253yuv_Sensor.h"
#include "hi253yuv_Camera_Sensor_para.h"
#include "hi253yuv_CameraCustomized.h" 

#define HI253YUV_DEBUG
#ifdef HI253YUV_DEBUG
#define SENSORDB printk
#else
#define SENSORDB(x,...)
#endif

struct
{
  kal_bool    NightMode;
  kal_uint8   ZoomFactor; /* Zoom Index */
  kal_uint16  Banding;
  kal_uint32  PvShutter;
  kal_uint32  PvDummyPixels;
  kal_uint32  PvDummyLines;
  kal_uint32  CapDummyPixels;
  kal_uint32  CapDummyLines;
  kal_uint32  PvOpClk;
  kal_uint32  CapOpClk;
  
  /* Video frame rate 300 means 30.0fps. Unit Multiple 10. */
  kal_uint32  MaxFrameRate; 
  kal_uint32  MiniFrameRate; 
  /* Sensor Register backup. */
  kal_uint8   VDOCTL2; /* P0.0x11. */
  kal_uint8   ISPCTL3; /* P10.0x12. */
  kal_uint8   AECTL1;  /* P20.0x10. */
  kal_uint8   AWBCTL1; /* P22.0x10. */
} HI253Status;

static DEFINE_SPINLOCK(hi253_drv_lock);


extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 * a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);

#define Sleep(ms) mdelay(ms)

kal_uint16 HI253WriteCmosSensor(kal_uint32 Addr, kal_uint32 Para)
{
  char pSendCmd[2] = {(char)(Addr & 0xFF) ,(char)(Para & 0xFF)};

  iWriteRegI2C(pSendCmd , 2,HI253_WRITE_ID);
}

kal_uint16 HI253ReadCmosSensor(kal_uint32 Addr)
{
  char pGetByte=0;
  char pSendCmd = (char)(Addr & 0xFF);
  
  iReadRegI2C(&pSendCmd , 1, &pGetByte,1,HI253_WRITE_ID);
  
  return pGetByte;
}

void HI253SetPage(kal_uint8 Page)
{
  HI253WriteCmosSensor(0x03, Page);
}

void HI253InitSetting(void)
{
  /*[SENSOR_INITIALIZATION]
    Base init is this driver's own (stable framing, correct luma).
    Only the colour-processing pages P0x11/P0x13/P0x14/P0x15/P0x16/P0x22
    (AWB, colour-correction-matrix and gamma) are overlaid 1:1 from the
    Lenovo A60+ stock kernel (IDA sub_3438C0). Readout timing, windowing,
    PLL, black level and exposure are left untouched. */

  HI253WriteCmosSensor(0x01, 0xF9);
  HI253WriteCmosSensor(0x08, 0x0F);
  HI253WriteCmosSensor(0x01, 0xF8);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x0E, 0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x0E, 0x00);
  HI253WriteCmosSensor(0x01, 0xF1);
  HI253WriteCmosSensor(0x08, 0x00);
  HI253WriteCmosSensor(0x01, 0xF3);
  HI253WriteCmosSensor(0x01, 0xF1);
  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x10, 0x0C);
  HI253SetPage(0x22);
  /* stock A60+ page 0x22 colour block */
  HI253WriteCmosSensor(0x10, 0xFD);
  HI253WriteCmosSensor(0x11, 0x2E);
  HI253WriteCmosSensor(0x19, 0x01);
  HI253WriteCmosSensor(0x20, 0x30);
  HI253WriteCmosSensor(0x21, 0x80);
  HI253WriteCmosSensor(0x24, 0x01);
  HI253WriteCmosSensor(0x30, 0x80);
  HI253WriteCmosSensor(0x31, 0x80);
  HI253WriteCmosSensor(0x38, 0x11);
  HI253WriteCmosSensor(0x39, 0x34);
  HI253WriteCmosSensor(0x40, 0xF7);
  HI253WriteCmosSensor(0x41, 0x55);
  HI253WriteCmosSensor(0x42, 0x33);
  HI253WriteCmosSensor(0x43, 0xF7);
  HI253WriteCmosSensor(0x44, 0x55);
  HI253WriteCmosSensor(0x45, 0x44);
  HI253WriteCmosSensor(0x46, 0x00);
  HI253WriteCmosSensor(0x50, 0xB2);
  HI253WriteCmosSensor(0x51, 0x81);
  HI253WriteCmosSensor(0x52, 0x98);
  HI253WriteCmosSensor(0x80, 0x40);
  HI253WriteCmosSensor(0x81, 0x20);
  HI253WriteCmosSensor(0x82, 0x3E);
  HI253WriteCmosSensor(0x83, 0x5E);
  HI253WriteCmosSensor(0x84, 0x1E);
  HI253WriteCmosSensor(0x85, 0x5E);
  HI253WriteCmosSensor(0x86, 0x22);
  HI253WriteCmosSensor(0x87, 0x4B);
  HI253WriteCmosSensor(0x88, 0x30);
  HI253WriteCmosSensor(0x89, 0x3F);
  HI253WriteCmosSensor(0x8A, 0x26);
  HI253WriteCmosSensor(0x8B, 0x4B);
  HI253WriteCmosSensor(0x8C, 0x30);
  HI253WriteCmosSensor(0x8D, 0x39);
  HI253WriteCmosSensor(0x8E, 0x26);
  HI253WriteCmosSensor(0x8F, 0x53);
  HI253WriteCmosSensor(0x90, 0x52);
  HI253WriteCmosSensor(0x91, 0x51);
  HI253WriteCmosSensor(0x92, 0x4E);
  HI253WriteCmosSensor(0x93, 0x4A);
  HI253WriteCmosSensor(0x94, 0x45);
  HI253WriteCmosSensor(0x95, 0x3D);
  HI253WriteCmosSensor(0x96, 0x31);
  HI253WriteCmosSensor(0x97, 0x28);
  HI253WriteCmosSensor(0x98, 0x24);
  HI253WriteCmosSensor(0x99, 0x20);
  HI253WriteCmosSensor(0x9A, 0x20);
  HI253WriteCmosSensor(0x9B, 0x99);
  HI253WriteCmosSensor(0x9C, 0x99);
  HI253WriteCmosSensor(0x9D, 0x48);
  HI253WriteCmosSensor(0x9E, 0x38);
  HI253WriteCmosSensor(0x9F, 0x30);
  HI253WriteCmosSensor(0xA0, 0x60);
  HI253WriteCmosSensor(0xA1, 0x34);
  HI253WriteCmosSensor(0xA2, 0x6F);
  HI253WriteCmosSensor(0xA3, 0xFF);
  HI253WriteCmosSensor(0xA4, 0x14);
  HI253WriteCmosSensor(0xA5, 0x2C);
  HI253WriteCmosSensor(0xA6, 0xCF);
  HI253WriteCmosSensor(0xAD, 0x40);
  HI253WriteCmosSensor(0xAE, 0x4A);
  HI253WriteCmosSensor(0xAF, 0x28);
  HI253WriteCmosSensor(0xB0, 0x26);
  HI253WriteCmosSensor(0xB1, 0x00);
  HI253WriteCmosSensor(0xB4, 0xEA);
  HI253WriteCmosSensor(0xB8, 0xA0);
  HI253WriteCmosSensor(0xB9, 0x00);
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x10, 0x13);
  HI253WriteCmosSensor(0x11, 0x90);
  HI253WriteCmosSensor(0x12, 0x04);
  HI253WriteCmosSensor(0x0B, 0xAA);
  HI253WriteCmosSensor(0x0C, 0xAA);
  HI253WriteCmosSensor(0x0D, 0xAA);
  HI253WriteCmosSensor(0x20, 0x00);
  HI253WriteCmosSensor(0x21, 0x04);
  HI253WriteCmosSensor(0x22, 0x00);
  HI253WriteCmosSensor(0x23, 0x07);
  HI253WriteCmosSensor(0x24, 0x04);
  HI253WriteCmosSensor(0x25, 0xB0);
  HI253WriteCmosSensor(0x26, 0x06);
  HI253WriteCmosSensor(0x27, 0x40);
  HI253WriteCmosSensor(0x40, 0x01);
  HI253WriteCmosSensor(0x41, 0xA8);
  HI253WriteCmosSensor(0x42, 0x00);
  HI253WriteCmosSensor(0x43, 0x3E);
  HI253WriteCmosSensor(0x45, 0x04);
  HI253WriteCmosSensor(0x46, 0x18);
  HI253WriteCmosSensor(0x47, 0xD8);
  HI253WriteCmosSensor(0xE1, 0x0F);
  HI253WriteCmosSensor(0x80, 0x2E);
  HI253WriteCmosSensor(0x81, 0x7E);
  HI253WriteCmosSensor(0x82, 0x90);
  HI253WriteCmosSensor(0x83, 0x00);
  HI253WriteCmosSensor(0x84, 0x0C);
  HI253WriteCmosSensor(0x85, 0x00);
  HI253WriteCmosSensor(0x90, 0x0A);
  HI253WriteCmosSensor(0x91, 0x0A);
  HI253WriteCmosSensor(0x92, 0x78);
  HI253WriteCmosSensor(0x93, 0x70);
  HI253WriteCmosSensor(0x94, 0x75);
  HI253WriteCmosSensor(0x95, 0x70);
  HI253WriteCmosSensor(0x96, 0xDC);
  HI253WriteCmosSensor(0x97, 0xFE);
  HI253WriteCmosSensor(0x98, 0x20);
  HI253WriteCmosSensor(0x99, 0x42);
  HI253WriteCmosSensor(0x9A, 0x42);
  HI253WriteCmosSensor(0x9B, 0x42);
  HI253WriteCmosSensor(0x9C, 0x42);
  HI253WriteCmosSensor(0xA0, 0x00);
  HI253WriteCmosSensor(0xA2, 0x00);
  HI253WriteCmosSensor(0xA4, 0x00);
  HI253WriteCmosSensor(0xA6, 0x00);
  HI253WriteCmosSensor(0xA8, 0x43);
  HI253WriteCmosSensor(0xAA, 0x43);
  HI253WriteCmosSensor(0xAC, 0x43);
  HI253WriteCmosSensor(0xAE, 0x43);
  HI253SetPage(0x02);
  HI253WriteCmosSensor(0x12, 0x03);
  HI253WriteCmosSensor(0x13, 0x03);
  HI253WriteCmosSensor(0x16, 0x00);
  HI253WriteCmosSensor(0x17, 0x8C);
  HI253WriteCmosSensor(0x18, 0x4C);
  HI253WriteCmosSensor(0x19, 0x00);
  HI253WriteCmosSensor(0x1A, 0x39);
  HI253WriteCmosSensor(0x1C, 0x09);
  HI253WriteCmosSensor(0x1D, 0x40);
  HI253WriteCmosSensor(0x1E, 0x30);
  HI253WriteCmosSensor(0x1F, 0x10);
  HI253WriteCmosSensor(0x20, 0x77);
  HI253WriteCmosSensor(0x21, 0xDE);
  HI253WriteCmosSensor(0x22, 0xA7);
  HI253WriteCmosSensor(0x23, 0x30);
  HI253WriteCmosSensor(0x27, 0x3C);
  HI253WriteCmosSensor(0x2B, 0x80);
  HI253WriteCmosSensor(0x2E, 0x00);
  HI253WriteCmosSensor(0x2F, 0x00);
  HI253WriteCmosSensor(0x30, 0x05);
  HI253WriteCmosSensor(0x50, 0x20);
  HI253WriteCmosSensor(0x52, 0x01);
  HI253WriteCmosSensor(0x53, 0xC1);
  HI253WriteCmosSensor(0x55, 0x1C);
  HI253WriteCmosSensor(0x56, 0x11);
  HI253WriteCmosSensor(0x5D, 0xA2);
  HI253WriteCmosSensor(0x5E, 0x5A);
  HI253WriteCmosSensor(0x60, 0x87);
  HI253WriteCmosSensor(0x61, 0x99);
  HI253WriteCmosSensor(0x62, 0x88);
  HI253WriteCmosSensor(0x63, 0x97);
  HI253WriteCmosSensor(0x64, 0x88);
  HI253WriteCmosSensor(0x65, 0x97);
  HI253WriteCmosSensor(0x67, 0x0C);
  HI253WriteCmosSensor(0x68, 0x0C);
  HI253WriteCmosSensor(0x69, 0x0C);
  HI253WriteCmosSensor(0x72, 0x89);
  HI253WriteCmosSensor(0x73, 0x96);
  HI253WriteCmosSensor(0x74, 0x89);
  HI253WriteCmosSensor(0x75, 0x96);
  HI253WriteCmosSensor(0x76, 0x89);
  HI253WriteCmosSensor(0x77, 0x96);
  HI253WriteCmosSensor(0x7C, 0x85);
  HI253WriteCmosSensor(0x7D, 0xAF);
  HI253WriteCmosSensor(0x80, 0x01);
  HI253WriteCmosSensor(0x81, 0x7F);
  HI253WriteCmosSensor(0x82, 0x13);
  HI253WriteCmosSensor(0x83, 0x24);
  HI253WriteCmosSensor(0x84, 0x7D);
  HI253WriteCmosSensor(0x85, 0x81);
  HI253WriteCmosSensor(0x86, 0x7D);
  HI253WriteCmosSensor(0x87, 0x81);
  HI253WriteCmosSensor(0x92, 0x48);
  HI253WriteCmosSensor(0x93, 0x54);
  HI253WriteCmosSensor(0x94, 0x7D);
  HI253WriteCmosSensor(0x95, 0x81);
  HI253WriteCmosSensor(0x96, 0x7D);
  HI253WriteCmosSensor(0x97, 0x81);
  HI253WriteCmosSensor(0xA0, 0x02);
  HI253WriteCmosSensor(0xA1, 0x7B);
  HI253WriteCmosSensor(0xA2, 0x02);
  HI253WriteCmosSensor(0xA3, 0x7B);
  HI253WriteCmosSensor(0xA4, 0x7B);
  HI253WriteCmosSensor(0xA5, 0x02);
  HI253WriteCmosSensor(0xA6, 0x7B);
  HI253WriteCmosSensor(0xA7, 0x02);
  HI253WriteCmosSensor(0xA8, 0x85);
  HI253WriteCmosSensor(0xA9, 0x8C);
  HI253WriteCmosSensor(0xAA, 0x85);
  HI253WriteCmosSensor(0xAB, 0x8C);
  HI253WriteCmosSensor(0xAC, 0x10);
  HI253WriteCmosSensor(0xAD, 0x16);
  HI253WriteCmosSensor(0xAE, 0x10);
  HI253WriteCmosSensor(0xAF, 0x16);
  HI253WriteCmosSensor(0xB0, 0x99);
  HI253WriteCmosSensor(0xB1, 0xA3);
  HI253WriteCmosSensor(0xB2, 0xA4);
  HI253WriteCmosSensor(0xB3, 0xAE);
  HI253WriteCmosSensor(0xB4, 0x9B);
  HI253WriteCmosSensor(0xB5, 0xA2);
  HI253WriteCmosSensor(0xB6, 0xA6);
  HI253WriteCmosSensor(0xB7, 0xAC);
  HI253WriteCmosSensor(0xB8, 0x9B);
  HI253WriteCmosSensor(0xB9, 0x9F);
  HI253WriteCmosSensor(0xBA, 0xA6);
  HI253WriteCmosSensor(0xBB, 0xAA);
  HI253WriteCmosSensor(0xBC, 0x9B);
  HI253WriteCmosSensor(0xBD, 0x9F);
  HI253WriteCmosSensor(0xBE, 0xA6);
  HI253WriteCmosSensor(0xBF, 0xAA);
  HI253WriteCmosSensor(0xC4, 0x2C);
  HI253WriteCmosSensor(0xC5, 0x43);
  HI253WriteCmosSensor(0xC6, 0x63);
  HI253WriteCmosSensor(0xC7, 0x79);
  HI253WriteCmosSensor(0xC8, 0x2D);
  HI253WriteCmosSensor(0xC9, 0x42);
  HI253WriteCmosSensor(0xCA, 0x2D);
  HI253WriteCmosSensor(0xCB, 0x42);
  HI253WriteCmosSensor(0xCC, 0x64);
  HI253WriteCmosSensor(0xCD, 0x78);
  HI253WriteCmosSensor(0xCE, 0x64);
  HI253WriteCmosSensor(0xCF, 0x78);
  HI253WriteCmosSensor(0xD0, 0x0A);
  HI253WriteCmosSensor(0xD1, 0x09);
  HI253WriteCmosSensor(0xD4, 0x0A);
  HI253WriteCmosSensor(0xD5, 0x0A);
  HI253WriteCmosSensor(0xD6, 0x78);
  HI253WriteCmosSensor(0xD7, 0x70);
  HI253WriteCmosSensor(0xE0, 0xC4);
  HI253WriteCmosSensor(0xE1, 0xC4);
  HI253WriteCmosSensor(0xE2, 0xC4);
  HI253WriteCmosSensor(0xE3, 0xC4);
  HI253WriteCmosSensor(0xE4, 0x00);
  HI253WriteCmosSensor(0xE8, 0x80);
  HI253WriteCmosSensor(0xE9, 0x40);
  HI253WriteCmosSensor(0xEA, 0x7F);
  HI253WriteCmosSensor(0xF0, 0x01);
  HI253WriteCmosSensor(0xF1, 0x01);
  HI253WriteCmosSensor(0xF2, 0x01);
  HI253WriteCmosSensor(0xF3, 0x01);
  HI253WriteCmosSensor(0xF4, 0x01);
  HI253SetPage(0x03);
  HI253WriteCmosSensor(0x10, 0x10);
  HI253SetPage(0x10);
  HI253WriteCmosSensor(0x10, 0x03);
  HI253WriteCmosSensor(0x12, 0x30);
  HI253WriteCmosSensor(0x20, 0x00);
  HI253WriteCmosSensor(0x30, 0x00);
  HI253WriteCmosSensor(0x31, 0x00);
  HI253WriteCmosSensor(0x32, 0x00);
  HI253WriteCmosSensor(0x33, 0x00);
  HI253WriteCmosSensor(0x34, 0x30);
  HI253WriteCmosSensor(0x35, 0x00);
  HI253WriteCmosSensor(0x36, 0x00);
  HI253WriteCmosSensor(0x38, 0x00);
  HI253WriteCmosSensor(0x3E, 0x58);
  HI253WriteCmosSensor(0x3F, 0x02);
  HI253WriteCmosSensor(0x40, 0x85);
  HI253WriteCmosSensor(0x41, 0x00);
  HI253WriteCmosSensor(0x60, 0x6F);
  HI253WriteCmosSensor(0x61, 0x76);
  HI253WriteCmosSensor(0x62, 0x76);
  HI253WriteCmosSensor(0x63, 0x30);
  HI253WriteCmosSensor(0x64, 0x41);
  HI253WriteCmosSensor(0x66, 0x33);
  HI253WriteCmosSensor(0x67, 0x00);
  HI253WriteCmosSensor(0x6A, 0x90);
  HI253WriteCmosSensor(0x6B, 0x80);
  HI253WriteCmosSensor(0x6C, 0x80);
  HI253WriteCmosSensor(0x6D, 0xA0);
  HI253WriteCmosSensor(0x76, 0x01);
  HI253WriteCmosSensor(0x74, 0x66);
  HI253WriteCmosSensor(0x79, 0x06);
  HI253SetPage(0x11);
  /* stock A60+ page 0x11 colour block */
  HI253WriteCmosSensor(0x10, 0x7F);
  HI253WriteCmosSensor(0x11, 0x40);
  HI253WriteCmosSensor(0x12, 0x0A);
  HI253WriteCmosSensor(0x13, 0xBB);
  HI253WriteCmosSensor(0x26, 0x31);
  HI253WriteCmosSensor(0x27, 0x34);
  HI253WriteCmosSensor(0x28, 0x0F);
  HI253WriteCmosSensor(0x29, 0x10);
  HI253WriteCmosSensor(0x2B, 0x30);
  HI253WriteCmosSensor(0x2C, 0x32);
  HI253WriteCmosSensor(0x30, 0x70);
  HI253WriteCmosSensor(0x31, 0x10);
  HI253WriteCmosSensor(0x32, 0x58);
  HI253WriteCmosSensor(0x33, 0x09);
  HI253WriteCmosSensor(0x34, 0x06);
  HI253WriteCmosSensor(0x35, 0x03);
  HI253WriteCmosSensor(0x36, 0x70);
  HI253WriteCmosSensor(0x37, 0x18);
  HI253WriteCmosSensor(0x38, 0x58);
  HI253WriteCmosSensor(0x39, 0x09);
  HI253WriteCmosSensor(0x3A, 0x06);
  HI253WriteCmosSensor(0x3B, 0x03);
  HI253WriteCmosSensor(0x3C, 0x80);
  HI253WriteCmosSensor(0x3D, 0x18);
  HI253WriteCmosSensor(0x3E, 0xA0);
  HI253WriteCmosSensor(0x3F, 0x0C);
  HI253WriteCmosSensor(0x40, 0x09);
  HI253WriteCmosSensor(0x41, 0x06);
  HI253WriteCmosSensor(0x42, 0x80);
  HI253WriteCmosSensor(0x43, 0x18);
  HI253WriteCmosSensor(0x44, 0xA0);
  HI253WriteCmosSensor(0x45, 0x12);
  HI253WriteCmosSensor(0x46, 0x10);
  HI253WriteCmosSensor(0x47, 0x10);
  HI253WriteCmosSensor(0x48, 0x90);
  HI253WriteCmosSensor(0x49, 0x40);
  HI253WriteCmosSensor(0x4A, 0x80);
  HI253WriteCmosSensor(0x4B, 0x13);
  HI253WriteCmosSensor(0x4C, 0x10);
  HI253WriteCmosSensor(0x4D, 0x11);
  HI253WriteCmosSensor(0x4E, 0x80);
  HI253WriteCmosSensor(0x4F, 0x30);
  HI253WriteCmosSensor(0x50, 0x80);
  HI253WriteCmosSensor(0x51, 0x13);
  HI253WriteCmosSensor(0x52, 0x10);
  HI253WriteCmosSensor(0x53, 0x13);
  HI253WriteCmosSensor(0x54, 0x11);
  HI253WriteCmosSensor(0x55, 0x17);
  HI253WriteCmosSensor(0x56, 0x20);
  HI253WriteCmosSensor(0x57, 0x01);
  HI253WriteCmosSensor(0x58, 0x00);
  HI253WriteCmosSensor(0x59, 0x00);
  HI253WriteCmosSensor(0x5A, 0x1F);
  HI253WriteCmosSensor(0x5B, 0x00);
  HI253WriteCmosSensor(0x5C, 0x00);
  HI253WriteCmosSensor(0x60, 0x3F);
  HI253WriteCmosSensor(0x62, 0x60);
  HI253WriteCmosSensor(0x70, 0x06);
  HI253SetPage(0x12);
  HI253WriteCmosSensor(0x20, 0x00);
  HI253WriteCmosSensor(0x21, 0x00);
  HI253WriteCmosSensor(0x25, 0x30);
  HI253WriteCmosSensor(0x28, 0x00);
  HI253WriteCmosSensor(0x29, 0x00);
  HI253WriteCmosSensor(0x2A, 0x00);
  HI253WriteCmosSensor(0x30, 0x50);
  HI253WriteCmosSensor(0x31, 0x18);
  HI253WriteCmosSensor(0x32, 0x32);
  HI253WriteCmosSensor(0x33, 0x40);
  HI253WriteCmosSensor(0x34, 0x50);
  HI253WriteCmosSensor(0x35, 0x70);
  HI253WriteCmosSensor(0x36, 0xA0);
  HI253WriteCmosSensor(0x3B, 0x06);
  HI253WriteCmosSensor(0x3C, 0x06);
  HI253WriteCmosSensor(0x40, 0xA0);
  HI253WriteCmosSensor(0x41, 0x40);
  HI253WriteCmosSensor(0x42, 0xA0);
  HI253WriteCmosSensor(0x43, 0x90);
  HI253WriteCmosSensor(0x44, 0x90);
  HI253WriteCmosSensor(0x45, 0x80);
  HI253WriteCmosSensor(0x46, 0xB0);
  HI253WriteCmosSensor(0x47, 0x55);
  HI253WriteCmosSensor(0x48, 0xA0);
  HI253WriteCmosSensor(0x49, 0x90);
  HI253WriteCmosSensor(0x4A, 0x90);
  HI253WriteCmosSensor(0x4B, 0x80);
  HI253WriteCmosSensor(0x4C, 0xB0);
  HI253WriteCmosSensor(0x4D, 0x40);
  HI253WriteCmosSensor(0x4E, 0x90);
  HI253WriteCmosSensor(0x4F, 0x90);
  HI253WriteCmosSensor(0x50, 0xE6);
  HI253WriteCmosSensor(0x51, 0x80);
  HI253WriteCmosSensor(0x52, 0xB0);
  HI253WriteCmosSensor(0x53, 0x60);
  HI253WriteCmosSensor(0x54, 0xC0);
  HI253WriteCmosSensor(0x55, 0xC0);
  HI253WriteCmosSensor(0x56, 0xC0);
  HI253WriteCmosSensor(0x57, 0x80);
  HI253WriteCmosSensor(0x58, 0x90);
  HI253WriteCmosSensor(0x59, 0x40);
  HI253WriteCmosSensor(0x5A, 0xD0);
  HI253WriteCmosSensor(0x5B, 0xD0);
  HI253WriteCmosSensor(0x5C, 0xE0);
  HI253WriteCmosSensor(0x5D, 0x80);
  HI253WriteCmosSensor(0x5E, 0x88);
  HI253WriteCmosSensor(0x5F, 0x40);
  HI253WriteCmosSensor(0x60, 0xE0);
  HI253WriteCmosSensor(0x61, 0xE6);
  HI253WriteCmosSensor(0x62, 0xE6);
  HI253WriteCmosSensor(0x63, 0x80);
  HI253WriteCmosSensor(0x70, 0x15);
  HI253WriteCmosSensor(0x71, 0x01);
  HI253WriteCmosSensor(0x72, 0x18);
  HI253WriteCmosSensor(0x73, 0x01);
  HI253WriteCmosSensor(0x74, 0x25);
  HI253WriteCmosSensor(0x75, 0x15);
  HI253WriteCmosSensor(0x80, 0x30);
  HI253WriteCmosSensor(0x81, 0x50);
  HI253WriteCmosSensor(0x82, 0x80);
  HI253WriteCmosSensor(0x85, 0x1A);
  HI253WriteCmosSensor(0x88, 0x00);
  HI253WriteCmosSensor(0x89, 0x00);
  HI253WriteCmosSensor(0x90, 0x00);
  HI253WriteCmosSensor(0xC5, 0x30);
  HI253WriteCmosSensor(0xC6, 0x2A);
  HI253WriteCmosSensor(0xD0, 0x0C);
  HI253WriteCmosSensor(0xD1, 0x80);
  HI253WriteCmosSensor(0xD2, 0x67);
  HI253WriteCmosSensor(0xD3, 0x00);
  HI253WriteCmosSensor(0xD4, 0x00);
  HI253WriteCmosSensor(0xD5, 0x02);
  HI253WriteCmosSensor(0xD6, 0xFF);
  HI253WriteCmosSensor(0xD7, 0x18);
  HI253SetPage(0x13);
  /* stock A60+ page 0x13 colour block */
  HI253WriteCmosSensor(0x10, 0xCB);
  HI253WriteCmosSensor(0x11, 0x7B);
  HI253WriteCmosSensor(0x12, 0x07);
  HI253WriteCmosSensor(0x14, 0x00);
  HI253WriteCmosSensor(0x20, 0x15);
  HI253WriteCmosSensor(0x21, 0x13);
  HI253WriteCmosSensor(0x22, 0x33);
  HI253WriteCmosSensor(0x23, 0x05);
  HI253WriteCmosSensor(0x24, 0x09);
  HI253WriteCmosSensor(0x25, 0x0A);
  HI253WriteCmosSensor(0x26, 0x18);
  HI253WriteCmosSensor(0x27, 0x30);
  HI253WriteCmosSensor(0x29, 0x12);
  HI253WriteCmosSensor(0x2A, 0x50);
  HI253WriteCmosSensor(0x2B, 0x00);
  HI253WriteCmosSensor(0x2C, 0x00);
  HI253WriteCmosSensor(0x25, 0x06);
  HI253WriteCmosSensor(0x2D, 0x0C);
  HI253WriteCmosSensor(0x2E, 0x12);
  HI253WriteCmosSensor(0x2F, 0x12);
  HI253WriteCmosSensor(0x50, 0x18);
  HI253WriteCmosSensor(0x51, 0x1C);
  HI253WriteCmosSensor(0x52, 0x1A);
  HI253WriteCmosSensor(0x53, 0x14);
  HI253WriteCmosSensor(0x54, 0x17);
  HI253WriteCmosSensor(0x55, 0x14);
  HI253WriteCmosSensor(0x56, 0x18);
  HI253WriteCmosSensor(0x57, 0x1C);
  HI253WriteCmosSensor(0x58, 0x1A);
  HI253WriteCmosSensor(0x59, 0x14);
  HI253WriteCmosSensor(0x5A, 0x17);
  HI253WriteCmosSensor(0x5B, 0x14);
  HI253WriteCmosSensor(0x5C, 0x0A);
  HI253WriteCmosSensor(0x5D, 0x0B);
  HI253WriteCmosSensor(0x5E, 0x0A);
  HI253WriteCmosSensor(0x5F, 0x08);
  HI253WriteCmosSensor(0x60, 0x09);
  HI253WriteCmosSensor(0x61, 0x08);
  HI253WriteCmosSensor(0x62, 0x08);
  HI253WriteCmosSensor(0x63, 0x08);
  HI253WriteCmosSensor(0x64, 0x08);
  HI253WriteCmosSensor(0x65, 0x06);
  HI253WriteCmosSensor(0x66, 0x06);
  HI253WriteCmosSensor(0x67, 0x06);
  HI253WriteCmosSensor(0x68, 0x07);
  HI253WriteCmosSensor(0x69, 0x07);
  HI253WriteCmosSensor(0x6A, 0x07);
  HI253WriteCmosSensor(0x6B, 0x05);
  HI253WriteCmosSensor(0x6C, 0x05);
  HI253WriteCmosSensor(0x6D, 0x05);
  HI253WriteCmosSensor(0x6E, 0x07);
  HI253WriteCmosSensor(0x6F, 0x07);
  HI253WriteCmosSensor(0x70, 0x07);
  HI253WriteCmosSensor(0x71, 0x05);
  HI253WriteCmosSensor(0x72, 0x05);
  HI253WriteCmosSensor(0x73, 0x05);
  HI253WriteCmosSensor(0x80, 0xFD);
  HI253WriteCmosSensor(0x81, 0x1F);
  HI253WriteCmosSensor(0x82, 0x05);
  HI253WriteCmosSensor(0x83, 0x31);
  HI253WriteCmosSensor(0x90, 0x05);
  HI253WriteCmosSensor(0x91, 0x05);
  HI253WriteCmosSensor(0x92, 0x33);
  HI253WriteCmosSensor(0x93, 0x30);
  HI253WriteCmosSensor(0x94, 0x03);
  HI253WriteCmosSensor(0x95, 0x14);
  HI253WriteCmosSensor(0x97, 0x20);
  HI253WriteCmosSensor(0x99, 0x20);
  HI253WriteCmosSensor(0xA0, 0x01);
  HI253WriteCmosSensor(0xA1, 0x02);
  HI253WriteCmosSensor(0xA2, 0x01);
  HI253WriteCmosSensor(0xA3, 0x02);
  HI253WriteCmosSensor(0xA4, 0x05);
  HI253WriteCmosSensor(0xA5, 0x05);
  HI253WriteCmosSensor(0xA6, 0x07);
  HI253WriteCmosSensor(0xA7, 0x08);
  HI253WriteCmosSensor(0xA8, 0x07);
  HI253WriteCmosSensor(0xA9, 0x08);
  HI253WriteCmosSensor(0xAA, 0x07);
  HI253WriteCmosSensor(0xAB, 0x08);
  HI253WriteCmosSensor(0xB0, 0x22);
  HI253WriteCmosSensor(0xB1, 0x2A);
  HI253WriteCmosSensor(0xB2, 0x28);
  HI253WriteCmosSensor(0xB3, 0x22);
  HI253WriteCmosSensor(0xB4, 0x2A);
  HI253WriteCmosSensor(0xB5, 0x28);
  HI253WriteCmosSensor(0xB6, 0x22);
  HI253WriteCmosSensor(0xB7, 0x2A);
  HI253WriteCmosSensor(0xB8, 0x28);
  HI253WriteCmosSensor(0xB9, 0x22);
  HI253WriteCmosSensor(0xBA, 0x2A);
  HI253WriteCmosSensor(0xBB, 0x28);
  HI253WriteCmosSensor(0xBC, 0x25);
  HI253WriteCmosSensor(0xBD, 0x2A);
  HI253WriteCmosSensor(0xBE, 0x27);
  HI253WriteCmosSensor(0xBF, 0x25);
  HI253WriteCmosSensor(0xC0, 0x2A);
  HI253WriteCmosSensor(0xC1, 0x27);
  HI253WriteCmosSensor(0xC2, 0x1E);
  HI253WriteCmosSensor(0xC3, 0x24);
  HI253WriteCmosSensor(0xC4, 0x20);
  HI253WriteCmosSensor(0xC5, 0x1E);
  HI253WriteCmosSensor(0xC6, 0x24);
  HI253WriteCmosSensor(0xC7, 0x20);
  HI253WriteCmosSensor(0xC8, 0x18);
  HI253WriteCmosSensor(0xC9, 0x20);
  HI253WriteCmosSensor(0xCA, 0x1E);
  HI253WriteCmosSensor(0xCB, 0x18);
  HI253WriteCmosSensor(0xCC, 0x20);
  HI253WriteCmosSensor(0xCD, 0x1E);
  HI253WriteCmosSensor(0xCE, 0x18);
  HI253WriteCmosSensor(0xCF, 0x20);
  HI253WriteCmosSensor(0xD0, 0x1E);
  HI253WriteCmosSensor(0xD1, 0x18);
  HI253WriteCmosSensor(0xD2, 0x20);
  HI253WriteCmosSensor(0xD3, 0x1E);
  HI253SetPage(0x14);
  /* stock A60+ page 0x14 colour block */
  HI253WriteCmosSensor(0x10, 0x01);
  HI253WriteCmosSensor(0x14, 0xB8);
  HI253WriteCmosSensor(0x15, 0x80);
  HI253WriteCmosSensor(0x16, 0xA0);
  HI253WriteCmosSensor(0x17, 0x80);
  HI253WriteCmosSensor(0x18, 0xB8);
  HI253WriteCmosSensor(0x19, 0x80);
  HI253WriteCmosSensor(0x20, 0xA0);
  HI253WriteCmosSensor(0x21, 0x80);
  HI253WriteCmosSensor(0x22, 0xA0);
  HI253WriteCmosSensor(0x23, 0x8E);
  HI253WriteCmosSensor(0x24, 0x84);
  HI253WriteCmosSensor(0x30, 0xC8);
  HI253WriteCmosSensor(0x31, 0x2B);
  HI253WriteCmosSensor(0x32, 0x00);
  HI253WriteCmosSensor(0x33, 0x00);
  HI253WriteCmosSensor(0x34, 0x90);
  HI253WriteCmosSensor(0x40, 0x5E);
  HI253WriteCmosSensor(0x50, 0x4E);
  HI253WriteCmosSensor(0x60, 0x4F);
  HI253WriteCmosSensor(0x70, 0x4E);
  HI253SetPage(0x15);
  /* stock A60+ page 0x15 colour block */
  HI253WriteCmosSensor(0x10, 0x0F);
  HI253WriteCmosSensor(0x14, 0x42);
  HI253WriteCmosSensor(0x15, 0x32);
  HI253WriteCmosSensor(0x16, 0x24);
  HI253WriteCmosSensor(0x17, 0x2F);
  HI253WriteCmosSensor(0x30, 0x8F);
  HI253WriteCmosSensor(0x31, 0x59);
  HI253WriteCmosSensor(0x32, 0x0A);
  HI253WriteCmosSensor(0x33, 0x15);
  HI253WriteCmosSensor(0x34, 0x5B);
  HI253WriteCmosSensor(0x35, 0x06);
  HI253WriteCmosSensor(0x36, 0x07);
  HI253WriteCmosSensor(0x37, 0x40);
  HI253WriteCmosSensor(0x38, 0x87);
  HI253WriteCmosSensor(0x40, 0x92);
  HI253WriteCmosSensor(0x41, 0x1B);
  HI253WriteCmosSensor(0x42, 0x89);
  HI253WriteCmosSensor(0x43, 0x81);
  HI253WriteCmosSensor(0x44, 0x00);
  HI253WriteCmosSensor(0x45, 0x01);
  HI253WriteCmosSensor(0x46, 0x89);
  HI253WriteCmosSensor(0x47, 0x9E);
  HI253WriteCmosSensor(0x48, 0x28);
  HI253WriteCmosSensor(0x50, 0x02);
  HI253WriteCmosSensor(0x51, 0x82);
  HI253WriteCmosSensor(0x52, 0x00);
  HI253WriteCmosSensor(0x53, 0x07);
  HI253WriteCmosSensor(0x54, 0x11);
  HI253WriteCmosSensor(0x55, 0x98);
  HI253WriteCmosSensor(0x56, 0x00);
  HI253WriteCmosSensor(0x57, 0x0B);
  HI253WriteCmosSensor(0x58, 0x8B);
  HI253WriteCmosSensor(0x80, 0x03);
  HI253WriteCmosSensor(0x85, 0x40);
  HI253WriteCmosSensor(0x87, 0x02);
  HI253WriteCmosSensor(0x88, 0x00);
  HI253WriteCmosSensor(0x89, 0x00);
  HI253WriteCmosSensor(0x8A, 0x00);
  HI253SetPage(0x16);
  /* stock A60+ page 0x16 colour block */
  HI253WriteCmosSensor(0x10, 0x31);
  HI253WriteCmosSensor(0x18, 0x5E);
  HI253WriteCmosSensor(0x19, 0x5D);
  HI253WriteCmosSensor(0x1A, 0x0E);
  HI253WriteCmosSensor(0x1B, 0x01);
  HI253WriteCmosSensor(0x1C, 0xDC);
  HI253WriteCmosSensor(0x1D, 0xFE);
  HI253WriteCmosSensor(0x30, 0x00);
  HI253WriteCmosSensor(0x31, 0x1F);
  HI253WriteCmosSensor(0x32, 0x30);
  HI253WriteCmosSensor(0x33, 0x54);
  HI253WriteCmosSensor(0x34, 0x72);
  HI253WriteCmosSensor(0x35, 0x86);
  HI253WriteCmosSensor(0x36, 0x95);
  HI253WriteCmosSensor(0x37, 0xA4);
  HI253WriteCmosSensor(0x38, 0xB3);
  HI253WriteCmosSensor(0x39, 0xC1);
  HI253WriteCmosSensor(0x3A, 0xCC);
  HI253WriteCmosSensor(0x3B, 0xD6);
  HI253WriteCmosSensor(0x3C, 0xDF);
  HI253WriteCmosSensor(0x3D, 0xE7);
  HI253WriteCmosSensor(0x3E, 0xED);
  HI253WriteCmosSensor(0x3F, 0xF3);
  HI253WriteCmosSensor(0x40, 0xF8);
  HI253WriteCmosSensor(0x41, 0xFC);
  HI253WriteCmosSensor(0x42, 0xFF);
  HI253WriteCmosSensor(0x50, 0x00);
  HI253WriteCmosSensor(0x51, 0x09);
  HI253WriteCmosSensor(0x52, 0x1F);
  HI253WriteCmosSensor(0x53, 0x37);
  HI253WriteCmosSensor(0x54, 0x5B);
  HI253WriteCmosSensor(0x55, 0x76);
  HI253WriteCmosSensor(0x56, 0x8D);
  HI253WriteCmosSensor(0x57, 0xA1);
  HI253WriteCmosSensor(0x58, 0xB2);
  HI253WriteCmosSensor(0x59, 0xBE);
  HI253WriteCmosSensor(0x5A, 0xC9);
  HI253WriteCmosSensor(0x5B, 0xD2);
  HI253WriteCmosSensor(0x5C, 0xDB);
  HI253WriteCmosSensor(0x5D, 0xE3);
  HI253WriteCmosSensor(0x5E, 0xEB);
  HI253WriteCmosSensor(0x5F, 0xF0);
  HI253WriteCmosSensor(0x60, 0xF5);
  HI253WriteCmosSensor(0x61, 0xF7);
  HI253WriteCmosSensor(0x62, 0xF8);
  HI253WriteCmosSensor(0x70, 0x00);
  HI253WriteCmosSensor(0x71, 0x0C);
  HI253WriteCmosSensor(0x72, 0x14);
  HI253WriteCmosSensor(0x73, 0x22);
  HI253WriteCmosSensor(0x74, 0x3C);
  HI253WriteCmosSensor(0x75, 0x55);
  HI253WriteCmosSensor(0x76, 0x6B);
  HI253WriteCmosSensor(0x77, 0x81);
  HI253WriteCmosSensor(0x78, 0x94);
  HI253WriteCmosSensor(0x79, 0xA6);
  HI253WriteCmosSensor(0x7A, 0xB6);
  HI253WriteCmosSensor(0x7B, 0xC4);
  HI253WriteCmosSensor(0x7C, 0xD1);
  HI253WriteCmosSensor(0x7D, 0xDB);
  HI253WriteCmosSensor(0x7E, 0xE5);
  HI253WriteCmosSensor(0x7F, 0xED);
  HI253WriteCmosSensor(0x80, 0xF4);
  HI253WriteCmosSensor(0x81, 0xFA);
  HI253WriteCmosSensor(0x82, 0xFF);
  HI253SetPage(0x17);
  HI253WriteCmosSensor(0xC4, 0x68);
  HI253WriteCmosSensor(0xC5, 0x56);
  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x11, 0x1C);
  HI253WriteCmosSensor(0x20, 0x01);
  HI253WriteCmosSensor(0x21, 0x30);
  HI253WriteCmosSensor(0x22, 0x10);
  HI253WriteCmosSensor(0x23, 0x00);
  HI253WriteCmosSensor(0x24, 0x04);
  HI253WriteCmosSensor(0x28, 0xFF);
  HI253WriteCmosSensor(0x29, 0xAD);
  HI253WriteCmosSensor(0x2A, 0xF0);
  HI253WriteCmosSensor(0x2B, 0x34);
  HI253WriteCmosSensor(0x2C, 0xC3);
  HI253WriteCmosSensor(0x2D, 0x5F);
  HI253WriteCmosSensor(0x2E, 0x33);
  HI253WriteCmosSensor(0x30, 0x78);
  HI253WriteCmosSensor(0x32, 0x03);
  HI253WriteCmosSensor(0x33, 0x2E);
  HI253WriteCmosSensor(0x34, 0x30);
  HI253WriteCmosSensor(0x35, 0xD4);
  HI253WriteCmosSensor(0x36, 0xFE);
  HI253WriteCmosSensor(0x37, 0x32);
  HI253WriteCmosSensor(0x38, 0x04);
  HI253WriteCmosSensor(0x3B, 0x22);
  HI253WriteCmosSensor(0x3C, 0xEF);
  HI253WriteCmosSensor(0x47, 0xF0);
  HI253WriteCmosSensor(0x50, 0x45);
  HI253WriteCmosSensor(0x51, 0x88);
  HI253WriteCmosSensor(0x56, 0x10);
  HI253WriteCmosSensor(0x57, 0xB7);
  HI253WriteCmosSensor(0x58, 0x14);
  HI253WriteCmosSensor(0x59, 0x88);
  HI253WriteCmosSensor(0x5A, 0x04);
  HI253WriteCmosSensor(0x60, 0x55);
  HI253WriteCmosSensor(0x61, 0x55);
  HI253WriteCmosSensor(0x62, 0x6A);
  HI253WriteCmosSensor(0x63, 0xA9);
  HI253WriteCmosSensor(0x64, 0x6A);
  HI253WriteCmosSensor(0x65, 0xA9);
  HI253WriteCmosSensor(0x66, 0x6A);
  HI253WriteCmosSensor(0x67, 0xA9);
  HI253WriteCmosSensor(0x68, 0x6B);
  HI253WriteCmosSensor(0x69, 0xE9);
  HI253WriteCmosSensor(0x6A, 0x6A);
  HI253WriteCmosSensor(0x6B, 0xA9);
  HI253WriteCmosSensor(0x6C, 0x6A);
  HI253WriteCmosSensor(0x6D, 0xA9);
  HI253WriteCmosSensor(0x6E, 0x55);
  HI253WriteCmosSensor(0x6F, 0x55);
  HI253WriteCmosSensor(0x70, 0x46);
  HI253WriteCmosSensor(0x71, 0xBB);
  HI253WriteCmosSensor(0x76, 0x21);
  HI253WriteCmosSensor(0x77, 0xBC);
  HI253WriteCmosSensor(0x78, 0x34);
  HI253WriteCmosSensor(0x79, 0x3A);
  HI253WriteCmosSensor(0x7A, 0x23);
  HI253WriteCmosSensor(0x7B, 0x22);
  HI253WriteCmosSensor(0x7D, 0x23);
  HI253WriteCmosSensor(0x83, 0x01);
  HI253WriteCmosSensor(0x84, 0x7C);
  HI253WriteCmosSensor(0x85, 0x40);
  HI253WriteCmosSensor(0x86, 0x01);
  HI253WriteCmosSensor(0x87, 0x38);
  HI253WriteCmosSensor(0x88, 0x04);
  HI253WriteCmosSensor(0x89, 0xF3);
  HI253WriteCmosSensor(0x8A, 0x80);
  HI253WriteCmosSensor(0x8B, 0x7E);
  HI253WriteCmosSensor(0x8C, 0xC0);
  HI253WriteCmosSensor(0x8D, 0x69);
  HI253WriteCmosSensor(0x8E, 0x6C);
  HI253WriteCmosSensor(0x91, 0x05);
  HI253WriteCmosSensor(0x92, 0xE9);
  HI253WriteCmosSensor(0x93, 0xAC);
  HI253WriteCmosSensor(0x94, 0x04);
  HI253WriteCmosSensor(0x95, 0x32);
  HI253WriteCmosSensor(0x96, 0x38);
  HI253WriteCmosSensor(0x98, 0xDC);
  HI253WriteCmosSensor(0x99, 0x45);
  HI253WriteCmosSensor(0x9A, 0x0D);
  HI253WriteCmosSensor(0x9B, 0xDE);
  HI253WriteCmosSensor(0x9C, 0x07);
  HI253WriteCmosSensor(0x9D, 0x50);
  HI253WriteCmosSensor(0x9E, 0x01);
  HI253WriteCmosSensor(0x9F, 0x38);
  HI253WriteCmosSensor(0xA0, 0x03);
  HI253WriteCmosSensor(0xA1, 0xA9);
  HI253WriteCmosSensor(0xA2, 0x80);
  HI253WriteCmosSensor(0xB0, 0x1D);
  HI253WriteCmosSensor(0xB1, 0x1A);
  HI253WriteCmosSensor(0xB2, 0x80);
  HI253WriteCmosSensor(0xB3, 0x20);
  HI253WriteCmosSensor(0xB4, 0x1A);
  HI253WriteCmosSensor(0xB5, 0x44);
  HI253WriteCmosSensor(0xB6, 0x2F);
  HI253WriteCmosSensor(0xB7, 0x28);
  HI253WriteCmosSensor(0xB8, 0x25);
  HI253WriteCmosSensor(0xB9, 0x22);
  HI253WriteCmosSensor(0xBA, 0x21);
  HI253WriteCmosSensor(0xBB, 0x20);
  HI253WriteCmosSensor(0xBC, 0x1F);
  HI253WriteCmosSensor(0xBD, 0x1F);
  HI253WriteCmosSensor(0xC0, 0x30);
  HI253WriteCmosSensor(0xC1, 0x20);
  HI253WriteCmosSensor(0xC2, 0x20);
  HI253WriteCmosSensor(0xC3, 0x20);
  HI253WriteCmosSensor(0xC4, 0x08);
  HI253WriteCmosSensor(0xC8, 0x80);
  HI253WriteCmosSensor(0xC9, 0x40);
  HI253SetPage(0x22);
  HI253WriteCmosSensor(0x10, 0xFD);
  HI253WriteCmosSensor(0x11, 0x2E);
  HI253WriteCmosSensor(0x19, 0x01);
  HI253WriteCmosSensor(0x20, 0x30);
  HI253WriteCmosSensor(0x21, 0x80);
  HI253WriteCmosSensor(0x23, 0x08);
  HI253WriteCmosSensor(0x24, 0x01);
  HI253WriteCmosSensor(0x30, 0x80);
  HI253WriteCmosSensor(0x31, 0x80);
  HI253WriteCmosSensor(0x38, 0x11);
  HI253WriteCmosSensor(0x39, 0x34);
  HI253WriteCmosSensor(0x40, 0xF7);
  HI253WriteCmosSensor(0x41, 0x77);
  HI253WriteCmosSensor(0x42, 0x55);
  HI253WriteCmosSensor(0x43, 0xF0);
  HI253WriteCmosSensor(0x44, 0x66);
  HI253WriteCmosSensor(0x45, 0x33);
  HI253WriteCmosSensor(0x46, 0x01);
  HI253WriteCmosSensor(0x47, 0x94);
  HI253WriteCmosSensor(0x50, 0xB2);
  HI253WriteCmosSensor(0x51, 0x81);
  HI253WriteCmosSensor(0x52, 0x98);
  HI253WriteCmosSensor(0x80, 0x38);
  HI253WriteCmosSensor(0x81, 0x20);
  HI253WriteCmosSensor(0x82, 0x38);
  HI253WriteCmosSensor(0x83, 0x5E);
  HI253WriteCmosSensor(0x84, 0x20);
  HI253WriteCmosSensor(0x85, 0x53);
  HI253WriteCmosSensor(0x86, 0x15);
  HI253WriteCmosSensor(0x87, 0x54);
  HI253WriteCmosSensor(0x88, 0x20);
  HI253WriteCmosSensor(0x89, 0x3F);
  HI253WriteCmosSensor(0x8A, 0x1C);
  HI253WriteCmosSensor(0x8B, 0x54);
  HI253WriteCmosSensor(0x8C, 0x3F);
  HI253WriteCmosSensor(0x8D, 0x24);
  HI253WriteCmosSensor(0x8E, 0x1C);
  HI253WriteCmosSensor(0x8F, 0x60);
  HI253WriteCmosSensor(0x90, 0x5F);
  HI253WriteCmosSensor(0x91, 0x5C);
  HI253WriteCmosSensor(0x92, 0x4C);
  HI253WriteCmosSensor(0x93, 0x41);
  HI253WriteCmosSensor(0x94, 0x3B);
  HI253WriteCmosSensor(0x95, 0x36);
  HI253WriteCmosSensor(0x96, 0x30);
  HI253WriteCmosSensor(0x97, 0x27);
  HI253WriteCmosSensor(0x98, 0x20);
  HI253WriteCmosSensor(0x99, 0x1C);
  HI253WriteCmosSensor(0x9A, 0x19);
  HI253WriteCmosSensor(0x9B, 0x88);
  HI253WriteCmosSensor(0x9C, 0x88);
  HI253WriteCmosSensor(0x9D, 0x48);
  HI253WriteCmosSensor(0x9E, 0x38);
  HI253WriteCmosSensor(0x9F, 0x30);
  HI253WriteCmosSensor(0xA0, 0x74);
  HI253WriteCmosSensor(0xA1, 0x35);
  HI253WriteCmosSensor(0xA2, 0xAF);
  HI253WriteCmosSensor(0xA3, 0xF7);
  HI253WriteCmosSensor(0xA4, 0x10);
  HI253WriteCmosSensor(0xA5, 0x50);
  HI253WriteCmosSensor(0xA6, 0xC4);
  HI253WriteCmosSensor(0xAD, 0x40);
  HI253WriteCmosSensor(0xAE, 0x4A);
  HI253WriteCmosSensor(0xAF, 0x2A);
  HI253WriteCmosSensor(0xB0, 0x29);
  HI253WriteCmosSensor(0xB1, 0x20);
  HI253WriteCmosSensor(0xB4, 0xFF);
  HI253WriteCmosSensor(0xB8, 0x6B);
  HI253WriteCmosSensor(0xB9, 0x00);
  HI253SetPage(0x24);
  HI253WriteCmosSensor(0x10, 0x01);
  HI253WriteCmosSensor(0x18, 0x06);
  HI253WriteCmosSensor(0x30, 0x06);
  HI253WriteCmosSensor(0x31, 0x90);
  HI253WriteCmosSensor(0x32, 0x25);
  HI253WriteCmosSensor(0x33, 0xA2);
  HI253WriteCmosSensor(0x34, 0x26);
  HI253WriteCmosSensor(0x35, 0x58);
  HI253WriteCmosSensor(0x36, 0x60);
  HI253WriteCmosSensor(0x37, 0x00);
  HI253WriteCmosSensor(0x38, 0x50);
  HI253WriteCmosSensor(0x39, 0x00);
  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x10, 0x9C);
  HI253SetPage(0x22);
  HI253WriteCmosSensor(0x10, 0xE9);
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x0E, 0x03);
  HI253WriteCmosSensor(0x0E, 0x73);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x01, 0xF8);
}

void HI253InitPara(void)
{
	spin_lock(&hi253_drv_lock);
  HI253Status.NightMode = KAL_FALSE;
  HI253Status.ZoomFactor = 0;
  HI253Status.Banding = AE_FLICKER_MODE_50HZ;
  HI253Status.PvShutter = 0x17c40;
  HI253Status.MaxFrameRate = HI253_MAX_FPS;
  HI253Status.MiniFrameRate = HI253_FPS(10);
  HI253Status.PvDummyPixels = 424;
  HI253Status.PvDummyLines = 62;
  HI253Status.CapDummyPixels = 360;
  HI253Status.CapDummyLines = 52; /* 10 FPS, 104 for 9.6 FPS*/
  HI253Status.PvOpClk = 26;
  HI253Status.CapOpClk = 26;  
  HI253Status.VDOCTL2 = 0x90;
  HI253Status.ISPCTL3 = 0x30;
  HI253Status.AECTL1 = 0x9c;
  HI253Status.AWBCTL1 = 0xe9;
  	spin_unlock(&hi253_drv_lock);
}

/*************************************************************************
* FUNCTION
*  HI253SetMirror
*
* DESCRIPTION
*  This function mirror, flip or mirror & flip the sensor output image.
*
*  IMPORTANT NOTICE: For some sensor, it need re-set the output order Y1CbY2Cr after
*  mirror or flip.
*
* PARAMETERS
*  1. kal_uint16 : horizontal mirror or vertical flip direction.
*
* RETURNS
*  None
*
*************************************************************************/
static void HI253SetMirror(kal_uint16 ImageMirror)
{
	spin_lock(&hi253_drv_lock);
  HI253Status.VDOCTL2 &= 0xfc;   
  spin_unlock(&hi253_drv_lock);
  switch (ImageMirror)
  {
    case IMAGE_H_MIRROR:
		spin_lock(&hi253_drv_lock);
      HI253Status.VDOCTL2 |= 0x01;
	  spin_unlock(&hi253_drv_lock);
      break;
    case IMAGE_V_MIRROR:
		spin_lock(&hi253_drv_lock);
      HI253Status.VDOCTL2 |= 0x02; 
	  spin_unlock(&hi253_drv_lock);
      break;
    case IMAGE_HV_MIRROR:
		spin_lock(&hi253_drv_lock);
      HI253Status.VDOCTL2 |= 0x03;
	  spin_unlock(&hi253_drv_lock);
      break;
    case IMAGE_NORMAL:
    default:
		spin_lock(&hi253_drv_lock);
      HI253Status.VDOCTL2 |= 0x00; 
	  spin_unlock(&hi253_drv_lock);
  }
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x11,HI253Status.VDOCTL2);  
}

static void HI253SetAeMode(kal_bool AeEnable)
{
  SENSORDB("[HI253]HI253SetAeMode AeEnable:%d;\n",AeEnable);

  if (AeEnable == KAL_TRUE)
  {
  	spin_lock(&hi253_drv_lock);
    HI253Status.AECTL1 |= 0x80;
	spin_unlock(&hi253_drv_lock);
  }
  else
  {
  	spin_lock(&hi253_drv_lock);
    HI253Status.AECTL1 &= (~0x80);
	spin_unlock(&hi253_drv_lock);
  }
  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x10,HI253Status.AECTL1);  
}


static void HI253SetAwbMode(kal_bool AwbEnable)
{
  SENSORDB("[HI253]HI253SetAwbMode AwbEnable:%d;\n",AwbEnable);
  if (AwbEnable == KAL_TRUE)
  {
  	spin_lock(&hi253_drv_lock);
    HI253Status.AWBCTL1 |= 0x80;
	spin_unlock(&hi253_drv_lock);
  }
  else
  {
  	spin_lock(&hi253_drv_lock);
    HI253Status.AWBCTL1 &= (~0x80);
	spin_unlock(&hi253_drv_lock);
  }
  HI253SetPage(0x22);
  HI253WriteCmosSensor(0x10,HI253Status.AWBCTL1);  
}

/*************************************************************************
* FUNCTION
* HI253NightMode
*
* DESCRIPTION
* This function night mode of HI253.
*
* PARAMETERS
* none
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void HI253NightMode(kal_bool Enable)
{
  kal_uint32 EXPMAX, EXPTIME, BLC_TIME_TH_ONOFF;
  kal_uint32 LineLength,BandingValue;
  SENSORDB("[HI253]HI253NightMode Enable:%d;\n",Enable);
  /* Night mode only for camera preview */
  if (HI253Status.MaxFrameRate == HI253Status.MiniFrameRate)  return ;
  spin_lock(&hi253_drv_lock);
  HI253Status.MiniFrameRate = Enable ? HI253_FPS(5) : HI253_FPS(10);
  spin_unlock(&hi253_drv_lock);
  LineLength = HI253_PV_PERIOD_PIXEL_NUMS + HI253Status.PvDummyPixels;
  BandingValue = (HI253Status.Banding == AE_FLICKER_MODE_50HZ) ? 100 : 120;
  
  EXPTIME = (HI253Status.PvOpClk * 1000000 / LineLength / BandingValue) * BandingValue * LineLength * HI253_FRAME_RATE_UNIT / 8 / HI253Status.MaxFrameRate;
  EXPMAX = (HI253Status.PvOpClk * 1000000 / LineLength / BandingValue) * BandingValue * LineLength * HI253_FRAME_RATE_UNIT / 8 / HI253Status.MiniFrameRate;
  BLC_TIME_TH_ONOFF =  BandingValue * HI253_FRAME_RATE_UNIT / HI253Status.MiniFrameRate;

  SENSORDB("[HI253]LineLenght:%d,BandingValue:%d;MiniFrameRaet:%d\n",LineLength,BandingValue,HI253Status.MiniFrameRate);
  SENSORDB("[HI253]EXPTIME:%d; EXPMAX:%d; BLC_TIME_TH_ONOFF:%d;\n",EXPTIME,EXPMAX,BLC_TIME_TH_ONOFF);
  SENSORDB("[HI253]VDOCTL2:%x; AECTL1:%x; \n",HI253Status.VDOCTL2,HI253Status.AECTL1);

  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x01, 0xf9); // Sleep ON
  spin_lock(&hi253_drv_lock);
  HI253Status.VDOCTL2 &= 0xfb;   
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x11,HI253Status.VDOCTL2);  // Fixed frame rate OFF  
  HI253WriteCmosSensor(0x90, BLC_TIME_TH_ONOFF); // BLC_TIME_TH_ON
  HI253WriteCmosSensor(0x91, BLC_TIME_TH_ONOFF); // BLC_TIME_TH_OFF
  HI253WriteCmosSensor(0x92, 0x78); // BLC_AG_TH_ON
  HI253WriteCmosSensor(0x93, 0x70); // BLC_AG_TH_OFF
  HI253SetPage(0x02); 
  HI253WriteCmosSensor(0xd4, BLC_TIME_TH_ONOFF); // DCDC_TIME_TH_ON
  HI253WriteCmosSensor(0xd5, BLC_TIME_TH_ONOFF); // DCDC_TIME_TH_OFF
  HI253WriteCmosSensor(0xd6, 0x78); // DCDC_AG_TH_ON
  HI253WriteCmosSensor(0xd7, 0x70); // DCDC_AG_TH_OFF
  HI253SetPage(0x20);
  spin_lock(&hi253_drv_lock);
  HI253Status.AECTL1 &= (~0x80);  
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x10,HI253Status.AECTL1);// AE ON BIT 7    
  HI253WriteCmosSensor(0x18, 0x38); // AE Reset ON
  HI253WriteCmosSensor(0x11, 0x1c); // 0x35 for fixed frame rate
  HI253WriteCmosSensor(0x2a, 0xf0); // 0x35 for fixed frame rate
  HI253WriteCmosSensor(0x2b, 0x34); // 0x35 for fixed frame rate, 0x34 for dynamic frame rate  
  HI253WriteCmosSensor(0x83, (EXPTIME>>16)&(0xff)); // EXPTIMEH max fps
  HI253WriteCmosSensor(0x84, (EXPTIME>>8)&(0xff)); // EXPTIMEM
  HI253WriteCmosSensor(0x85, (EXPTIME>>0)&(0xff)); // EXPTIMEL
  HI253WriteCmosSensor(0x88, (EXPMAX>>16)&(0xff)); // EXPMAXH min fps init 0x04f380
  HI253WriteCmosSensor(0x89, (EXPMAX>>8)&(0xff)); // EXPMAXM
  HI253WriteCmosSensor(0x8a, (EXPMAX>>0)&(0xff)); // EXPMAXL
  HI253WriteCmosSensor(0x01, 0xf8); // Sleep OFF  
  spin_lock(&hi253_drv_lock);
  HI253Status.AECTL1 |= 0x80;   
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x10,HI253Status.AECTL1);// AE ON BIT 7    
  HI253WriteCmosSensor(0x18, 0x30); // AE Reset OFF
} /* HI253NightMode */


/*************************************************************************
* FUNCTION
* HI253Open
*
* DESCRIPTION
* this function initialize the registers of CMOS sensor
*
* PARAMETERS
* none
*
* RETURNS
*  none
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253Open(void)
{
  kal_uint16 SensorId = 0;
  //1 software reset sensor and wait (to sensor)
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x01,0xf1);
  HI253WriteCmosSensor(0x01,0xf3);
  HI253WriteCmosSensor(0x01,0xf1);

  SensorId = HI253ReadCmosSensor(0x04);
  Sleep(3);
  SENSORDB("[HI253]HI253Open: Sensor ID %x\n",SensorId);
  
  if(SensorId != HI253_SENSOR_ID)
  {
    return ERROR_SENSOR_CONNECT_FAIL;
  }
  HI253InitSetting();
  HI253InitPara();
  return ERROR_NONE;

}
/* HI253Open() */

/*************************************************************************
* FUNCTION
*   HI253GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID 
*
* PARAMETERS
*   *sensorID : return the sensor ID 
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253GetSensorID(UINT32 *sensorID) 
{
	//1 software reset sensor and wait (to sensor)
	HI253SetPage(0x00);
	HI253WriteCmosSensor(0x01,0xf1);
	HI253WriteCmosSensor(0x01,0xf3);
	HI253WriteCmosSensor(0x01,0xf1);
	
	*sensorID = HI253ReadCmosSensor(0x04);
	Sleep(3);
	SENSORDB("[HI253]HI253GetSensorID: Sensor ID %x\n",*sensorID);
	
	if(*sensorID != HI253_SENSOR_ID)
	{
        *sensorID = 0xFFFFFFFF; 
	  return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
* HI253Close
*
* DESCRIPTION
* This function is to turn off sensor module power.
*
* PARAMETERS
* None
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253Close(void)
{
  return ERROR_NONE;
} /* HI253Close() */

/*************************************************************************
* FUNCTION
* HI253Preview
*
* DESCRIPTION
* This function start the sensor preview.
*
* PARAMETERS
* *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
* None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 HI253Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
  kal_uint32 LineLength, EXP100, EXP120, EXPMIN, EXPUNIT; 

  SENSORDB("\n\n\n\n\n\n");
  SENSORDB("[HI253]HI253Preview\n");
  /* For change max frame rate only need modify HI253Status.MaxFrameRate */
  spin_lock(&hi253_drv_lock);
  HI253Status.MaxFrameRate = HI253_MAX_FPS;
  spin_unlock(&hi253_drv_lock);
  HI253SetMirror(IMAGE_NORMAL);

	spin_lock(&hi253_drv_lock);
  HI253Status.PvDummyPixels = 424;
  spin_unlock(&hi253_drv_lock);
  LineLength = HI253_PV_PERIOD_PIXEL_NUMS + HI253Status.PvDummyPixels;
  spin_lock(&hi253_drv_lock);
  HI253Status.MiniFrameRate = HI253_FPS(10);  
  HI253Status.PvDummyLines = HI253Status.PvOpClk * 1000000 * HI253_FRAME_RATE_UNIT / LineLength / HI253Status.MaxFrameRate -  HI253_PV_PERIOD_LINE_NUMS; 
  spin_unlock(&hi253_drv_lock);
  
  HI253SetPage(0x00); 
  HI253WriteCmosSensor(0x10, 0x13); 
  HI253WriteCmosSensor(0x12, 0x04); 
  HI253WriteCmosSensor(0x20, 0x00); // WINROWH
  HI253WriteCmosSensor(0x21, 0x04); // WINROWL
  HI253WriteCmosSensor(0x22, 0x00); // WINCOLH
  HI253WriteCmosSensor(0x23, 0x07); // WINCOLL
  
  HI253WriteCmosSensor(0x3f, 0x00);
  HI253WriteCmosSensor(0x40, (HI253Status.PvDummyPixels>>8)&0xff);
  HI253WriteCmosSensor(0x41, (HI253Status.PvDummyPixels>>0)&0xff);
  HI253WriteCmosSensor(0x42, (HI253Status.PvDummyLines>>8)&0xff);
  HI253WriteCmosSensor(0x43, (HI253Status.PvDummyLines>>0)&0xff); 
  HI253WriteCmosSensor(0x3f, 0x02);

  HI253SetPage(0x12);
  HI253WriteCmosSensor(0x20, 0x00);
  HI253WriteCmosSensor(0x21, 0x00);
  HI253WriteCmosSensor(0x90, 0x00);  
  HI253SetPage(0x13);
  HI253WriteCmosSensor(0x80, 0x00);

  EXP100 = (HI253Status.PvOpClk * 1000000 / LineLength) * LineLength / 100 / 8; 
  EXP120 = (HI253Status.PvOpClk * 1000000 / LineLength) * LineLength / 120 / 8; 
  EXPMIN = EXPUNIT = LineLength / 4; 

  SENSORDB("[HI253]DummyPixel:%d DummyLine:%d; LineLenght:%d,Plck:%d\n",HI253Status.PvDummyPixels,HI253Status.PvDummyLines,LineLength,HI253Status.PvOpClk);
  SENSORDB("[HI253]EXP100:%d EXP120:%d;\n",EXP100,EXP120);

  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x83, (HI253Status.PvShutter >> 16) & 0xFF);
  HI253WriteCmosSensor(0x84, (HI253Status.PvShutter >> 8) & 0xFF);
  HI253WriteCmosSensor(0x85, HI253Status.PvShutter & 0xFF);  
  HI253WriteCmosSensor(0x86, (EXPMIN>>8)&0xff);//EXPMIN
  HI253WriteCmosSensor(0x87, (EXPMIN>>0)&0xff);
  HI253WriteCmosSensor(0x8b, (EXP100>>8)&0xff);//EXP100
  HI253WriteCmosSensor(0x8c, (EXP100>>0)&0xff);
  HI253WriteCmosSensor(0x8d, (EXP120>>8)&0xff);//EXP120
  HI253WriteCmosSensor(0x8e, (EXP120>>0)&0xff);
  HI253WriteCmosSensor(0x9c, (HI253_PV_EXPOSURE_LIMITATION>>8)&0xff);
  HI253WriteCmosSensor(0x9d, (HI253_PV_EXPOSURE_LIMITATION>>0)&0xff);  
  HI253WriteCmosSensor(0x9e, (EXPUNIT>>8)&0xff);//EXP Unit
  HI253WriteCmosSensor(0x9f, (EXPUNIT>>0)&0xff);
  
  HI253SetAeMode(KAL_TRUE);
  HI253SetAwbMode(KAL_TRUE);
  
  return ERROR_NONE;
}/* HI253Preview() */

UINT32 HI253Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
  kal_uint32 LineLength, EXP100, EXP120, EXPMIN, EXPUNIT, CapShutter; 
  kal_uint8 ClockDivider;
  kal_uint32 temp;
  SENSORDB("\n\n\n\n\n\n");
  SENSORDB("[HI253]HI253Capture!!!!!!!!!!!!!\n");
  SENSORDB("[HI253]Image Target Width: %d; Height: %d\n",image_window->ImageTargetWidth, image_window->ImageTargetHeight);
  if ((image_window->ImageTargetWidth<=HI253_PV_WIDTH)&&
      (image_window->ImageTargetHeight<=HI253_PV_HEIGHT))
    return ERROR_NONE;    /* Less than PV Mode */

  HI253WriteCmosSensor(0x01, 0xf9); // Sleep ON
  HI253SetAeMode(KAL_FALSE);
  HI253SetAwbMode(KAL_FALSE);

  HI253SetPage(0x20);
  temp=(HI253ReadCmosSensor(0x80) << 16)|(HI253ReadCmosSensor(0x81) << 8)|HI253ReadCmosSensor(0x82);
  spin_lock(&hi253_drv_lock);
  HI253Status.PvShutter = temp;  
  spin_unlock(&hi253_drv_lock);

  // 1600*1200   
  HI253SetPage(0x00);  
  HI253WriteCmosSensor(0x10,0x00);
  HI253WriteCmosSensor(0x3f,0x00);  
  HI253SetPage(0x12); 
  HI253WriteCmosSensor(0x20, 0x0f);
  HI253WriteCmosSensor(0x21, 0x0f);
  HI253WriteCmosSensor(0x90, 0x5d);    
  HI253SetPage(0x13);
  HI253WriteCmosSensor(0x80, 0xfd);   
  /*capture 1600*1200 start x, y*/
  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x20, 0x00); // WINROWH
  HI253WriteCmosSensor(0x21, 0x0f); // WINROWL
  HI253WriteCmosSensor(0x22, 0x00); // WINCOLH
  HI253WriteCmosSensor(0x23, 0x19); // WINCOLL
  spin_lock(&hi253_drv_lock);
  HI253Status.CapDummyPixels = 360;
  HI253Status.CapDummyLines = 52; /* 10 FPS, 104 for 9.6 FPS*/
  spin_unlock(&hi253_drv_lock);
  LineLength = HI253_FULL_PERIOD_PIXEL_NUMS + HI253Status.CapDummyPixels;
  spin_lock(&hi253_drv_lock);
  HI253Status.CapOpClk = 26;  
  spin_unlock(&hi253_drv_lock);
  EXP100 = (HI253Status.CapOpClk * 1000000 / LineLength) * LineLength / 100 / 8; 
  EXP120 = (HI253Status.CapOpClk * 1000000 / LineLength) * LineLength / 120 / 8; 
  EXPMIN = EXPUNIT = LineLength / 4; 
  
  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x86, (EXPMIN>>8)&0xff);//EXPMIN
  HI253WriteCmosSensor(0x87, (EXPMIN>>0)&0xff);
  HI253WriteCmosSensor(0x8b, (EXP100>>8)&0xff);//EXP100
  HI253WriteCmosSensor(0x8c, (EXP100>>0)&0xff);
  HI253WriteCmosSensor(0x8d, (EXP120>>8)&0xff);//EXP120
  HI253WriteCmosSensor(0x8e, (EXP120>>0)&0xff);  
  HI253WriteCmosSensor(0x9c, (HI253_FULL_EXPOSURE_LIMITATION>>8)&0xff);
  HI253WriteCmosSensor(0x9d, (HI253_FULL_EXPOSURE_LIMITATION>>0)&0xff);  
  HI253WriteCmosSensor(0x9e, (EXPUNIT>>8)&0xff);//EXP Unit
  HI253WriteCmosSensor(0x9f, (EXPUNIT>>0)&0xff);

  HI253SetPage(0x00); 
  HI253WriteCmosSensor(0x40, (HI253Status.CapDummyPixels>>8)&0xff);
  HI253WriteCmosSensor(0x41, (HI253Status.CapDummyPixels>>0)&0xff);
  HI253WriteCmosSensor(0x42, (HI253Status.CapDummyLines>>8)&0xff);
  HI253WriteCmosSensor(0x43, (HI253Status.CapDummyLines>>0)&0xff);     

  if(HI253Status.ZoomFactor > 8)   
  {
    ClockDivider = 1; //Op CLock 13M
  }
  else
  {
    ClockDivider = 0; //OpCLock 26M
  }
  SENSORDB("[HI253]ClockDivider: %d \n",ClockDivider);
  HI253WriteCmosSensor(0x12, 0x04|ClockDivider);
  CapShutter = HI253Status.PvShutter >> ClockDivider;
  if(CapShutter<1)      CapShutter = 1;

  HI253SetPage(0x20);
  HI253WriteCmosSensor(0x83, (CapShutter >> 16) & 0xFF);
  HI253WriteCmosSensor(0x84, (CapShutter >> 8) & 0xFF);
  HI253WriteCmosSensor(0x85, CapShutter & 0xFF);  
  HI253WriteCmosSensor(0x01, 0xf8); // Sleep OFF  
  return ERROR_NONE;
} /* HI253Capture() */

UINT32 HI253GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
  pSensorResolution->SensorFullWidth = HI253_FULL_WIDTH;
  pSensorResolution->SensorFullHeight = HI253_FULL_HEIGHT;
  pSensorResolution->SensorPreviewWidth = HI253_PV_WIDTH;
  pSensorResolution->SensorPreviewHeight = HI253_PV_HEIGHT;
  return ERROR_NONE;
} /* HI253GetResolution() */

UINT32 HI253GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                    MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
  pSensorInfo->SensorPreviewResolutionX=HI253_PV_WIDTH;
  pSensorInfo->SensorPreviewResolutionY=HI253_PV_HEIGHT;
  pSensorInfo->SensorFullResolutionX=HI253_FULL_WIDTH;
  pSensorInfo->SensorFullResolutionY=HI253_FULL_HEIGHT;

  pSensorInfo->SensorCameraPreviewFrameRate=30;
  pSensorInfo->SensorVideoFrameRate=30;
  pSensorInfo->SensorStillCaptureFrameRate=10;
  pSensorInfo->SensorWebCamCaptureFrameRate=15;
  pSensorInfo->SensorResetActiveHigh=FALSE;
  pSensorInfo->SensorResetDelayCount=1;
  pSensorInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_YUYV; // back for 16 SENSOR_OUTPUT_FORMAT_UYVY;
  pSensorInfo->SensorClockPolarity=SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
  pSensorInfo->SensorInterruptDelayLines = 1;
  pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_PARALLEL;

  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].MaxWidth=CAM_SIZE_5M_WIDTH;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].MaxHeight=CAM_SIZE_5M_HEIGHT;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].ISOSupported=TRUE;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_100_MODE].BinningEnable=FALSE;

  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].MaxWidth=CAM_SIZE_5M_WIDTH;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].MaxHeight=CAM_SIZE_5M_HEIGHT;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].ISOSupported=TRUE;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_200_MODE].BinningEnable=FALSE;

  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].MaxWidth=CAM_SIZE_5M_WIDTH;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].MaxHeight=CAM_SIZE_5M_HEIGHT;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].ISOSupported=TRUE;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_400_MODE].BinningEnable=FALSE;

  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].MaxWidth=CAM_SIZE_1M_WIDTH;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].MaxHeight=CAM_SIZE_1M_HEIGHT;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].ISOSupported=TRUE;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_800_MODE].BinningEnable=TRUE;

  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].MaxWidth=CAM_SIZE_1M_WIDTH;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].MaxHeight=CAM_SIZE_1M_HEIGHT;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].ISOSupported=TRUE;
  pSensorInfo->SensorISOBinningInfo.ISOBinningInfo[ISO_1600_MODE].BinningEnable=TRUE;
  pSensorInfo->CaptureDelayFrame = 3; 
  pSensorInfo->PreviewDelayFrame = 3; 
  pSensorInfo->VideoDelayFrame = 4; 
  pSensorInfo->SensorMasterClockSwitch = 0; 
  pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA; 

  switch (ScenarioId)
  {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
    case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
    default:
      pSensorInfo->SensorClockFreq=26;
      pSensorInfo->SensorClockDividCount=3;
      pSensorInfo->SensorClockRisingCount=0;
      pSensorInfo->SensorClockFallingCount=2;
      pSensorInfo->SensorPixelClockCount=3;
      pSensorInfo->SensorDataLatchCount=2;
      pSensorInfo->SensorGrabStartX = HI253_GRAB_START_X; 
      pSensorInfo->SensorGrabStartY = HI253_GRAB_START_Y;
      break;
  }
  return ERROR_NONE;
} /* HI253GetInfo() */


UINT32 HI253Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
  switch (ScenarioId)
  {
  case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
  case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
  case MSDK_SCENARIO_ID_VIDEO_CAPTURE_MPEG4:
    HI253Preview(pImageWindow, pSensorConfigData);
    break;
  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
  case MSDK_SCENARIO_ID_CAMERA_CAPTURE_MEM:
    HI253Capture(pImageWindow, pSensorConfigData);
    break;
  default:
    break; 
  }
  return TRUE;
} /* HI253Control() */


BOOL HI253SetWb(UINT16 Para)
{
  SENSORDB("[HI253]HI253SetWb Para:%d;\n",Para);
  switch (Para)
  {
    case AWB_MODE_OFF:
      HI253SetAwbMode(KAL_FALSE);
      break;                     
    case AWB_MODE_AUTO:
      HI253SetAwbMode(KAL_TRUE);
      break;
    case AWB_MODE_CLOUDY_DAYLIGHT: //cloudy
      HI253SetAwbMode(KAL_FALSE);
      HI253WriteCmosSensor(0x80, 0x49);
      HI253WriteCmosSensor(0x81, 0x20);
      HI253WriteCmosSensor(0x82, 0x24);
      HI253WriteCmosSensor(0x83, 0x50);
      HI253WriteCmosSensor(0x84, 0x45);
      HI253WriteCmosSensor(0x85, 0x24);
      HI253WriteCmosSensor(0x86, 0x1e);
      break;
    case AWB_MODE_DAYLIGHT: //sunny
      HI253SetAwbMode(KAL_FALSE);
      HI253WriteCmosSensor(0x80, 0x45);
      HI253WriteCmosSensor(0x81, 0x20);
      HI253WriteCmosSensor(0x82, 0x27);
      HI253WriteCmosSensor(0x83, 0x44);
      HI253WriteCmosSensor(0x84, 0x3f);
      HI253WriteCmosSensor(0x85, 0x29);
      HI253WriteCmosSensor(0x86, 0x23);
      break;
    case AWB_MODE_INCANDESCENT: //office
      HI253SetAwbMode(KAL_FALSE);
      HI253WriteCmosSensor(0x80, 0x33);
      HI253WriteCmosSensor(0x81, 0x20);
      HI253WriteCmosSensor(0x82, 0x3d);
      HI253WriteCmosSensor(0x83, 0x2e);
      HI253WriteCmosSensor(0x84, 0x24);
      HI253WriteCmosSensor(0x85, 0x43);
      HI253WriteCmosSensor(0x86, 0x3d);
      break;
    case AWB_MODE_TUNGSTEN: //home
      HI253SetAwbMode(KAL_FALSE);
      HI253WriteCmosSensor(0x80, 0x25);
      HI253WriteCmosSensor(0x81, 0x20);
      HI253WriteCmosSensor(0x82, 0x44);
      HI253WriteCmosSensor(0x83, 0x22);
      HI253WriteCmosSensor(0x84, 0x1e);
      HI253WriteCmosSensor(0x85, 0x50);
      HI253WriteCmosSensor(0x86, 0x45);
      break;
    case AWB_MODE_FLUORESCENT:
      HI253SetAwbMode(KAL_FALSE);
      HI253WriteCmosSensor(0x80, 0x45);
      HI253WriteCmosSensor(0x81, 0x20);
      HI253WriteCmosSensor(0x82, 0x2f);
      HI253WriteCmosSensor(0x83, 0x38);
      HI253WriteCmosSensor(0x84, 0x32);
      HI253WriteCmosSensor(0x85, 0x39);
      HI253WriteCmosSensor(0x86, 0x33);
    default:
      return KAL_FALSE;
  }
  return KAL_TRUE;      
} /* HI253SetWb */

BOOL HI253SetEffect(UINT16 Para)
{
  SENSORDB("[HI253]HI253SetEffect Para:%d;\n",Para);
  switch (Para)
  {
    case MEFFECT_OFF:
      HI253SetPage(0x10);  
      HI253WriteCmosSensor(0x11, 0x03);
      HI253WriteCmosSensor(0x12, 0x30);
      HI253WriteCmosSensor(0x13, 0x00);
      HI253WriteCmosSensor(0x40, 0x00);
      break;
    case MEFFECT_SEPIA:
      HI253SetPage(0x10);  
      HI253WriteCmosSensor(0x11, 0x03);
      HI253WriteCmosSensor(0x12, 0x23);
      HI253WriteCmosSensor(0x13, 0x00);
      HI253WriteCmosSensor(0x40, 0x00);
      HI253WriteCmosSensor(0x44, 0x70);
      HI253WriteCmosSensor(0x45, 0x98);
      HI253WriteCmosSensor(0x47, 0x7f);
      HI253WriteCmosSensor(0x03, 0x13);
      HI253WriteCmosSensor(0x20, 0x07);
      HI253WriteCmosSensor(0x21, 0x07);
      break;
    case MEFFECT_NEGATIVE://----datasheet
      HI253SetPage(0x10);  
      HI253WriteCmosSensor(0x11, 0x03);
      HI253WriteCmosSensor(0x12, 0x28);
      HI253WriteCmosSensor(0x13, 0x00);
      HI253WriteCmosSensor(0x40, 0x00);
      HI253WriteCmosSensor(0x44, 0x80);
      HI253WriteCmosSensor(0x45, 0x80);
      HI253WriteCmosSensor(0x47, 0x7f);
      HI253WriteCmosSensor(0x03, 0x13);
      HI253WriteCmosSensor(0x20, 0x07);
      HI253WriteCmosSensor(0x21, 0x07);
      break;
    case MEFFECT_SEPIAGREEN://----datasheet aqua
      HI253SetPage(0x10);  
      HI253WriteCmosSensor(0x11, 0x03);
      HI253WriteCmosSensor(0x12, 0x23);
      HI253WriteCmosSensor(0x13, 0x00);
      HI253WriteCmosSensor(0x40, 0x00);
      HI253WriteCmosSensor(0x44, 0x80);
      HI253WriteCmosSensor(0x45, 0x04);
      HI253WriteCmosSensor(0x47, 0x7f);
      HI253WriteCmosSensor(0x03, 0x13);
      HI253WriteCmosSensor(0x20, 0x07);
      HI253WriteCmosSensor(0x21, 0x07);
      break;
    case MEFFECT_SEPIABLUE:
      HI253SetPage(0x10);  
      HI253WriteCmosSensor(0x11, 0x03);
      HI253WriteCmosSensor(0x12, 0x23);
      HI253WriteCmosSensor(0x13, 0x00);
      HI253WriteCmosSensor(0x40, 0x00);
      HI253WriteCmosSensor(0x44, 0xb0);
      HI253WriteCmosSensor(0x45, 0x40);
      HI253WriteCmosSensor(0x47, 0x7f);
      HI253WriteCmosSensor(0x03, 0x13);
      HI253WriteCmosSensor(0x20, 0x07);
      HI253WriteCmosSensor(0x21, 0x07);
      break;
    case MEFFECT_MONO: //----datasheet black & white
      HI253SetPage(0x10);  
      HI253WriteCmosSensor(0x11, 0x03);
      HI253WriteCmosSensor(0x12, 0x23);
      HI253WriteCmosSensor(0x13, 0x00);
      HI253WriteCmosSensor(0x40, 0x00);
      HI253WriteCmosSensor(0x44, 0x80);
      HI253WriteCmosSensor(0x45, 0x80);
      HI253WriteCmosSensor(0x47, 0x7f);
      HI253WriteCmosSensor(0x03, 0x13);
      HI253WriteCmosSensor(0x20, 0x07);
      HI253WriteCmosSensor(0x21, 0x07);
      break;
    default:
      return KAL_FALSE;
  }
  return KAL_TRUE;

} /* HI253SetEffect */

BOOL HI253SetBanding(UINT16 Para)
{
  SENSORDB("[HI253]HI253SetBanding Para:%d;\n",Para);
  spin_lock(&hi253_drv_lock);
  HI253Status.Banding = Para;
  spin_unlock(&hi253_drv_lock);
  if (HI253Status.Banding == AE_FLICKER_MODE_50HZ) 
  	{
  	spin_lock(&hi253_drv_lock);
    HI253Status.AECTL1 |= 0x10;
	spin_unlock(&hi253_drv_lock);
  	}
  else
  	{
  	spin_lock(&hi253_drv_lock);
    HI253Status.AECTL1 &= (~0x10); 
	spin_unlock(&hi253_drv_lock);
  	}
  
  HI253SetPage(0x20);  
  HI253WriteCmosSensor(0x10,HI253Status.AECTL1);
  return TRUE;
} /* HI253SetBanding */

BOOL HI253SetExposure(UINT16 Para)
{
  SENSORDB("[HI253]HI253SetExposure Para:%d;\n",Para);
  HI253SetPage(0x10);  
  spin_lock(&hi253_drv_lock);
  HI253Status.ISPCTL3 |= 0x10;
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x12,HI253Status.ISPCTL3);//make sure the Yoffset control is opened.
  
  switch (Para)
  {
    case AE_EV_COMP_n13:              /* EV -2 */
      HI253WriteCmosSensor(0x40,0xe0);
      break;
    case AE_EV_COMP_n10:              /* EV -1.5 */
      HI253WriteCmosSensor(0x40,0xc8);
      break;
    case AE_EV_COMP_n07:              /* EV -1 */
      HI253WriteCmosSensor(0x40,0xb0);
      break;
    case AE_EV_COMP_n03:              /* EV -0.5 */
      HI253WriteCmosSensor(0x40,0x98);
      break;
    case AE_EV_COMP_00:                /* EV 0 */
      HI253WriteCmosSensor(0x40,0x85);
      break;
    case AE_EV_COMP_03:              /* EV +0.5 */
      HI253WriteCmosSensor(0x40,0x18);
      break;
    case AE_EV_COMP_07:              /* EV +1 */
      HI253WriteCmosSensor(0x40,0x30);
      break;
    case AE_EV_COMP_10:              /* EV +1.5 */
      HI253WriteCmosSensor(0x40,0x48);
      break;
    case AE_EV_COMP_13:              /* EV +2 */
      HI253WriteCmosSensor(0x40,0x60);
      break;
    default:
      return KAL_FALSE;
  }
  return KAL_TRUE;
} /* HI253SetExposure */

UINT32 HI253YUVSensorSetting(FEATURE_ID Cmd, UINT32 Para)
{
  switch (Cmd) {
    case FID_SCENE_MODE:
      if (Para == SCENE_MODE_OFF)
      {
        HI253NightMode(KAL_FALSE); 
      }
      else if (Para == SCENE_MODE_NIGHTSCENE)
      {
        HI253NightMode(KAL_TRUE); 
      }  
      break; 
    case FID_AWB_MODE:
      HI253SetWb(Para);
      break;
    case FID_COLOR_EFFECT:
      HI253SetEffect(Para);
      break;
    case FID_AE_EV:
      HI253SetExposure(Para);
      break;
    case FID_AE_FLICKER:
      HI253SetBanding(Para);
      break;
    case FID_AE_SCENE_MODE: 
      if (Para == AE_MODE_OFF) 
      {
        HI253SetAeMode(KAL_FALSE);
      }
      else 
      {
        HI253SetAeMode(KAL_TRUE);
      }
      break; 
    case FID_ZOOM_FACTOR:
      SENSORDB("[HI253]ZoomFactor :%d;\n",Para);
	  spin_lock(&hi253_drv_lock);
      HI253Status.ZoomFactor = Para;
	  spin_unlock(&hi253_drv_lock);
      break;
    default:
      break;
  }
  return TRUE;
}   /* HI253YUVSensorSetting */

UINT32 HI253YUVSetVideoMode(UINT16 FrameRate)
{
  kal_uint32 EXPFIX, BLC_TIME_TH_ONOFF;
  kal_uint32 LineLength,BandingValue;
  
  SENSORDB("[HI253]HI253YUVSetVideoMode FrameRate:%d;\n",FrameRate);
  if (FrameRate * HI253_FRAME_RATE_UNIT > HI253_MAX_FPS)
    return -1;
  spin_lock(&hi253_drv_lock);
  HI253Status.MaxFrameRate = HI253Status.MiniFrameRate = FrameRate * HI253_FRAME_RATE_UNIT;
  spin_unlock(&hi253_drv_lock);
  LineLength = HI253_PV_PERIOD_PIXEL_NUMS + HI253Status.PvDummyPixels;
  BandingValue = (HI253Status.Banding == AE_FLICKER_MODE_50HZ) ? 100 : 120;
  
  EXPFIX = (HI253Status.PvOpClk * 1000000 / LineLength / BandingValue) * BandingValue * LineLength * HI253_FRAME_RATE_UNIT / 8 / HI253Status.MiniFrameRate;
  
  BLC_TIME_TH_ONOFF =  BandingValue * HI253_FRAME_RATE_UNIT / HI253Status.MiniFrameRate;

  SENSORDB("[HI253]LineLenght:%d,BandingValue:%d\n",LineLength,BandingValue);
  SENSORDB("[HI253]EXPFIX:%d BLC_TIME_TH_ONOFF:%d\n;",EXPFIX,BLC_TIME_TH_ONOFF);

  HI253SetPage(0x00);
  HI253WriteCmosSensor(0x01, 0xf9); // Sleep ON
  spin_lock(&hi253_drv_lock);
  HI253Status.VDOCTL2 |= 0x04;   
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x11,HI253Status.VDOCTL2);  // Fixed frame rate OFF    
  HI253WriteCmosSensor(0x90, BLC_TIME_TH_ONOFF); // BLC_TIME_TH_ON
  HI253WriteCmosSensor(0x91, BLC_TIME_TH_ONOFF); // BLC_TIME_TH_OFF
  HI253WriteCmosSensor(0x92, 0x78); // BLC_AG_TH_ON
  HI253WriteCmosSensor(0x93, 0x70); // BLC_AG_TH_OFF
  HI253WriteCmosSensor(0x03, 0x02); // Page 2
  HI253WriteCmosSensor(0xd4, BLC_TIME_TH_ONOFF); // DCDC_TIME_TH_ON
  HI253WriteCmosSensor(0xd5, BLC_TIME_TH_ONOFF); // DCDC_TIME_TH_OFF
  HI253WriteCmosSensor(0xd6, 0x78); // DCDC_AG_TH_ON
  HI253WriteCmosSensor(0xd7, 0x70); // DCDC_AG_TH_OFF
  
  HI253SetPage(0x20);
  spin_lock(&hi253_drv_lock);
  HI253Status.AECTL1 &= (~0x80);
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x10,HI253Status.AECTL1);// AE ON BIT 7    
  HI253WriteCmosSensor(0x18, 0x38); // AE Reset ON
  HI253WriteCmosSensor(0x11, 0x00); // 0x35 for fixed frame rate
  HI253WriteCmosSensor(0x2a, 0x03); // 0x35 for fixed frame rate
  HI253WriteCmosSensor(0x2b, 0x35); // 0x35 for fixed frame rate, 0x34 for dynamic frame rate  
  HI253WriteCmosSensor(0x83, (EXPFIX>>16)&(0xff)); // EXPTIMEH max fps
  HI253WriteCmosSensor(0x84, (EXPFIX>>8)&(0xff)); // EXPTIMEM
  HI253WriteCmosSensor(0x85, (EXPFIX>>0)&(0xff)); // EXPTIMEL
  HI253WriteCmosSensor(0x88, (EXPFIX>>16)&(0xff)); // EXPMAXH min fps
  HI253WriteCmosSensor(0x89, (EXPFIX>>8)&(0xff)); // EXPMAXM
  HI253WriteCmosSensor(0x8a, (EXPFIX>>0)&(0xff)); // EXPMAXL
  HI253WriteCmosSensor(0x91, (EXPFIX>>16)&(0xff)); // EXPMAXH min fps
  HI253WriteCmosSensor(0x92, (EXPFIX>>8)&(0xff)); // EXPMAXM
  HI253WriteCmosSensor(0x93, (EXPFIX>>0)&(0xff)); // EXPMAXL  
  HI253WriteCmosSensor(0x01, 0xf8); // Sleep OFF
  spin_lock(&hi253_drv_lock);
  HI253Status.AECTL1 |= 0x80;
  spin_unlock(&hi253_drv_lock);
  HI253WriteCmosSensor(0x10,HI253Status.AECTL1);// AE ON BIT 7    
  HI253WriteCmosSensor(0x18, 0x30); // AE Reset OFF  
  return TRUE;
}

UINT32 HI253FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                        UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
  UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
  UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
  UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
  UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
  MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
  MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;

  switch (FeatureId)
  {
    case SENSOR_FEATURE_GET_RESOLUTION:
      *pFeatureReturnPara16++=HI253_FULL_WIDTH;
      *pFeatureReturnPara16=HI253_FULL_HEIGHT;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_GET_PERIOD:
      *pFeatureReturnPara16++=HI253_PV_PERIOD_PIXEL_NUMS+HI253Status.PvDummyPixels;
      *pFeatureReturnPara16=HI253_PV_PERIOD_LINE_NUMS+HI253Status.PvDummyLines;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
      *pFeatureReturnPara32 = HI253Status.PvOpClk*2;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_SET_ESHUTTER:
      break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
      HI253NightMode((BOOL) *pFeatureData16);
      break;
    case SENSOR_FEATURE_SET_GAIN:
      case SENSOR_FEATURE_SET_FLASHLIGHT:
      break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
      break;
    case SENSOR_FEATURE_SET_REGISTER:
      HI253WriteCmosSensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
      break;
    case SENSOR_FEATURE_GET_REGISTER:
      pSensorRegData->RegData = HI253ReadCmosSensor(pSensorRegData->RegAddr);
      break;
    case SENSOR_FEATURE_GET_CONFIG_PARA:
      *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
      break;
    case SENSOR_FEATURE_SET_CCT_REGISTER:
    case SENSOR_FEATURE_GET_CCT_REGISTER:
    case SENSOR_FEATURE_SET_ENG_REGISTER:
    case SENSOR_FEATURE_GET_ENG_REGISTER:
    case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
    case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
    case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
    case SENSOR_FEATURE_GET_GROUP_INFO:
    case SENSOR_FEATURE_GET_ITEM_INFO:
    case SENSOR_FEATURE_SET_ITEM_INFO:
    case SENSOR_FEATURE_GET_ENG_INFO:
      break;
    case SENSOR_FEATURE_GET_GROUP_COUNT:
      *pFeatureReturnPara32++=0;
      *pFeatureParaLen=4;
      break; 
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
      // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
      // if EEPROM does not exist in camera module.
      *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
      *pFeatureParaLen=4;
      break;
    case SENSOR_FEATURE_SET_YUV_CMD:
      HI253YUVSensorSetting((FEATURE_ID)*pFeatureData32, *(pFeatureData32+1));
      break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
      HI253YUVSetVideoMode(*pFeatureData16);
      break; 
  case SENSOR_FEATURE_CHECK_SENSOR_ID:
	  HI253GetSensorID(pFeatureReturnPara32); 
	  break; 
    default:
      break;
  }
  return ERROR_NONE;
} /* HI253FeatureControl() */



UINT32 HI253_YUV_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
  static SENSOR_FUNCTION_STRUCT SensorFuncHI253=
  {
    HI253Open,
    HI253GetInfo,
    HI253GetResolution,
    HI253FeatureControl,
    HI253Control,
    HI253Close
  };

  /* To Do : Check Sensor status here */
  if (pfFunc!=NULL)
    *pfFunc=&SensorFuncHI253;

  return ERROR_NONE;
} /* SensorInit() */
