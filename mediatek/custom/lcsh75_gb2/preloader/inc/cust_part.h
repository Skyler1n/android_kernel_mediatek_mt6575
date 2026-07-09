#ifndef CUST_PART_H
#define CUST_PART_H

#include "partition_define.h"

#define PART_PRELOADER              "PRELOADER"
#define PART_DSP_BL                 "DSP_BL"
#define PART_MBOOT                  "MBOOT"
#define PART_MBL                    "MBL"
#define PART_MBR                    "MBR"
#define PART_EBR1                   "EBR1"
#define PART_EBR2                   "EBR2"
#define PART_UBOOT                  "UBOOT"
#define PART_SECURE                 "SECCNFG"
#define PART_LOGO                   "LOGO"
#define PART_CUST                   "CUSTOME"
#define PART_BACKUP                 "BACKUP"
#define PART_IMAGE                  "IMAGE"
#define PART_BOOTIMG                "BOOTIMG"
#define PART_USER                   "USER"
#define PART_ANDSYSIMG              "ANDSYSIMG"
#define PART_CACHE                  "CACHE"
#define PART_MISC                   "MISC"
#define PART_NVRAM                  "NVRAM"
#define PART_RECOVERY               "RECOVERY"
#define PART_EXPDB                  "EXPDB"
#define PART_AUTHEN_FILE            "AUTHEN_FILE"
#define PART_SECSTATIC              "SECSTATIC"
#define PART_KERNEL                 "KERNEL"
#define PART_ROOTFS                 "ROOTFS"
#define PART_AP_ROM                 "AP_ROM"
#define PART_MD_ROM                 "MD_ROM"
#define PART_DSP_ROM                "DSP_ROM"
#define PART_PMT               		"PMT"

#define PART_MAX_COUNT              20

#define PART_FLAG_NONE              0
#define PART_FLAG_LEFT              0x1
#define PART_FLAG_END               0x2
#define PART_MAGIC                  0x58881688

/*=======================================================================*/
/* Partitions                                                            */
/*=======================================================================*/
#define CFG_USE_MBL_PARTITION
#if 0
#define CFG_USE_AP_ROM_PARTITION
#define CFG_USE_MD_ROM_PARTITION
#define CFG_USE_DSP_ROM_PARTITION
#else
#define CFG_USE_UBOOT_PARTITION
#define CFG_USE_SECURE_PARTITION
#define CFG_USE_LOGO_PARTITION
#define CFG_USE_BOOTIMG_PARTITION
#define CFG_USE_RECOVERY_PARTITION
#define CFG_USE_SECSTATIC_PARTITION
#define CFG_USE_MISC_PARTITION
#define CFG_USE_NVRAM_PARTITION
#define CFG_USE_CACHE_PARTITION
#define CFG_USE_ANDROID_SYSIMG_PARTITION
#define CFG_USE_EXPDB_PARTITION
#define CFG_USE_USER_PARTITION
#endif

#ifdef MTK_EMMC_SUPPORT
#define CFG_USE_MBR_PARTITION
#define CFG_USE_EBR1_PARTITION
#define CFG_USE_EBR2_PARTITION
#define CFG_USE_PMT_PARTITION
#endif

typedef union
{
    struct
    {
        unsigned int magic;     /* partition magic */
        unsigned int dsize;     /* partition data size */
        char name[32];          /* partition name */    
        unsigned int maddr;     /* partition memory address */
    } info;
    unsigned char data[512];
} part_hdr_t;

typedef struct
{
    unsigned char *name;        /* partition name */
    unsigned long startblk;     /* partition start blk */
    unsigned long size;         /* partition size */
    unsigned long blks;         /* partition blks */
    unsigned long flags;        /* partition flags */
} part_t;

extern void cust_part_init(void);
extern part_t *cust_part_tbl(void);

#endif /* CUST_PART_H */
