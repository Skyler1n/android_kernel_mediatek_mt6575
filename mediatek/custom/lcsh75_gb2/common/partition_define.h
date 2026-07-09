#ifndef __PARTITION_DEFINE_H__
#define __PARTITION_DEFINE_H__




#define KB  (1024)
#define MB  (1024 * KB)
#define GB  (1024 * MB)

#define PART_SIZE_PRELOADER			(256*KB)
#define PART_SIZE_DSP_BL			(768*KB)
#define PART_SIZE_NVRAM			(3072*KB)
#define PART_SIZE_SECCFG			(128*KB)
#define PART_SIZE_UBOOT			(384*KB)
#define PART_SIZE_BOOTIMG			(5120*KB)
#define PART_SIZE_RECOVERY			(5120*KB)
#define PART_SIZE_SEC_RO			(1152*KB)
#define PART_SIZE_MISC			(384*KB)
#define PART_SIZE_LOGO			(3072*KB)
#define PART_SIZE_EXPDB			(640*KB)
#define PART_SIZE_FAT			(5120*KB)
#define PART_SIZE_ANDROID			(302080*KB)
#define PART_SIZE_CACHE			(61440*KB)
#define PART_SIZE_USRDATA			(0*KB)
#define PART_SIZE_BMTPOOL			(0x50)
#define PART_NUM			15
#define MBR_START_ADDRESS_BYTE			(*1024)



#endif
