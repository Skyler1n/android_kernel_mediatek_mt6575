/*****************************************************************************
*                E X T E R N A L      R E F E R E N C E S
******************************************************************************
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <mach/mt6575_gpio.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <mach/mt6575_typedefs.h>
#include <linux/pmic6326_sw.h>
#include <linux/delay.h>
#include <mach/mt6575_clock_manager.h>
#include <mach/mt6575_pmic_feature_api.h>
#include <board-custom.h>
#include "yusu_android_speaker.h"

/*****************************************************************************
*                C O M P I L E R      F L A G S
******************************************************************************
*/
//#define CONFIG_DEBUG_MSG
#ifdef CONFIG_DEBUG_MSG
#define PRINTK(format, args...) printk( KERN_EMERG format,##args )
#else
#define PRINTK(format, args...)
#endif
//#define AMP_CLASS_AB
#define AMP_CLASS_D
//#define ENABLE_2_IN_1_SPK

/*****************************************************************************
*                          C O N S T A N T S
******************************************************************************
*/

#define EXTERNAL_AMPLIFIER_GPIO (104)
#define SPK_WARM_UP_TIME        (55) //unit is ms
#define SPK_AMP_GAIN            (4)  //4:15dB
#define RCV_AMP_GAIN            (1)  //1:-3dB

#ifndef CUST_SPEAKER_TURNON
#define CUST_SPEAKER_TURNON() do {} while (0)
#endif

#ifndef CUST_SPEAKER_TURNOFF
#define CUST_SPEAKER_TURNOFF() do {} while (0)
#endif
/*****************************************************************************
*                         D A T A      T Y P E S
******************************************************************************
*/

static bool gsk_on=false; // speaker is open?
static bool gsk_resume=false;
static bool gsk_forceon=false;
/*****************************************************************************
*                  F U N C T I O N        D E F I N I T I O N
******************************************************************************
*/
extern void Yusu_Sound_AMP_Switch(BOOL enable);

bool Speaker_Init(void)
{
   PRINTK("+Speaker_Init Success");
#if defined(AMP_CLASS_AB)
   hwSPKClassABInit();
   hwSPKClassABVolumeControl(SPK_AMP_GAIN,SPK_AMP_GAIN);//4:15dB, 6:21dB
#else defined(AMP_CLASS_D)
   hwSPKClassDInit();
   hwSPKClassDVolumeControl(SPK_AMP_GAIN,SPK_AMP_GAIN);//4:15dB, 6:21dB
#endif
   hwSPKClassABVoiceBypassAnalogGain(RCV_AMP_GAIN);;
   //hwSPKClassABAnalogSwitchSelect(1);////1=>CH#2; 0=>CH#1
   PRINTK("-Speaker_Init Success");
   return true;
}

bool Speaker_DeInit(void)
{
	return false;
}

void Sound_SpeakerL_SetVolLevel(int level)
{
   PRINTK(" Sound_SpeakerL_SetVolLevel level=%d\n",level);
}

void Sound_SpeakerR_SetVolLevel(int level)
{
   PRINTK(" Sound_SpeakerR_SetVolLevel level=%d\n",level);
}

void Sound_Speaker_Turnon(int channel)
{
    PRINTK("Sound_Speaker_Turnon channel = %d\n",channel);
	if(gsk_on)
		return;
    hw2in1SpeakerSetting(0,0);//0=>spk mode; 1=>receiver mode
#if defined(AMP_CLASS_AB)
    hwSPKClassABTurnOn(1,1);
#else defined(AMP_CLASS_D)
    hwSPKClassDTurnOn(1,1);
#endif
    CUST_SPEAKER_TURNON();
    msleep(SPK_WARM_UP_TIME);
    gsk_on = true;
}

void Sound_Speaker_Turnoff(int channel)
{
    PRINTK("Sound_Speaker_Turnoff channel = %d\n",channel);
	if(!gsk_on)
		return;
    CUST_SPEAKER_TURNOFF();
#if defined(AMP_CLASS_AB)
    hwSPKClassABTurnOff();
#else defined(AMP_CLASS_D)
    hwSPKClassDTurnOff();
#endif
	gsk_on = false;
}

void Sound_Speaker_SetVolLevel(int level)
{
    Speaker_Volume =level;
}


void Sound_Headset_Turnon(void)
{
#if defined(ENABLE_2_IN_1_SPK)
#if defined(AMP_CLASS_D)
        hwSPKClassABInit();
#endif
        hw2in1SpeakerSetting(1,1);//0=>spk mode; 1=>receiver mode
        hwSPKClassABTurnOn();
#endif
}

void Sound_Headset_Turnoff(void)
{
#if defined(ENABLE_2_IN_1_SPK)
        hwSPKClassABTurnOff();
        //hw2in1SpeakerSetting(0,0);//0=>spk mode; 1=>receiver mode
#if defined(AMP_CLASS_D)
        hwSPKClassDInit();
#endif
#endif
}

//kernal use
void AudioAMPDevice_Suspend(void)
{
	PRINTK("AudioDevice_Suspend\n");
	if(gsk_on)
	{
		Sound_Speaker_Turnoff(Channel_Stereo);
		gsk_resume = true;
	}

}
void AudioAMPDevice_Resume(void)
{
	PRINTK("AudioDevice_Resume\n");
	if(gsk_resume)
		Sound_Speaker_Turnon(Channel_Stereo);
	gsk_resume = false;
}
void AudioAMPDevice_SpeakerLouderOpen(void)
{
	PRINTK("AudioDevice_SpeakerLouderOpen\n");
	gsk_forceon = false;
	if(gsk_on)
		return;
	Sound_Speaker_Turnon(Channel_Stereo);
	gsk_forceon = true;
	return ;

}
void AudioAMPDevice_SpeakerLouderClose(void)
{
	PRINTK("AudioDevice_SpeakerLouderClose\n");

	if(gsk_forceon)
		Sound_Speaker_Turnoff(Channel_Stereo);
	gsk_forceon = false;

}
void AudioAMPDevice_mute(void)
{
	PRINTK("AudioDevice_mute\n");
	if(gsk_on)
		Sound_Speaker_Turnoff(Channel_Stereo);
}

int Audio_eamp_command(unsigned int type, unsigned long args, unsigned int count)
{
	return 0;
}
static char *ExtFunArray[] =
{
    "InfoMATVAudioStart",
    "InfoMATVAudioStop",
    "End",
};

kal_int32 Sound_ExtFunction(const char* name, void* param, int param_size)
{
	int i = 0;
	int funNum = -1;

	//Search the supported function defined in ExtFunArray
	while(strcmp("End",ExtFunArray[i]) != 0 ) {		//while function not equal to "End"

	    if (strcmp(name,ExtFunArray[i]) == 0 ) {		//When function name equal to table, break
	    	funNum = i;
	    	break;
	    }
	    i++;
	}

	switch (funNum) {
	    case 0:			//InfoMATVAudioStart
	        printk("RunExtFunction InfoMATVAudioStart \n");
	        break;

	    case 1:			//InfoMATVAudioStop
	        printk("RunExtFunction InfoMATVAudioStop \n");
	        break;

	    default:
	    	 break;
	}

	return 1;
}

