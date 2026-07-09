#ifndef CUST_SEC_CTRL_H
#define CUST_SEC_CTRL_H

#include "typedefs.h"
#include "proj_cfg.h"

/**************************************************************************
 * [ROM INFO]
 **************************************************************************/
#define PROJECT_NAME                        "CUST"
#define PLATFORM_NAME                       "MT6575"

/**************************************************************************
 * [SEC ENV CONTROL]
 **************************************************************************/
#define SEC_ENV_ENABLE                      TRUE

/**************************************************************************
 * [CRYPTO SEED]
 **************************************************************************/
#define CUSTOM_CRYPTO_SEED_SIZE             (16)
#define CUSTOM_CRYPTO_SEED                  "1A52A367CB12C458"

/**************************************************************************
 * [S-USBDL]
 **************************************************************************/
/* S-USBDL Attribute */
#define ATTR_SUSBDL_DISABLE                 0x00
#define ATTR_SUSBDL_ENABLE                  0x11
#define ATTR_SUSBDL_ONLY_ENABLE_ON_SCHIP    0x22
/* S-USBDL Control */
#define SEC_USBDL_CFG                       CUSTOM_SUSBDL_CFG

/**************************************************************************
 * [S-BOOT]
 **************************************************************************/
/* S-BOOT Attribute */
#define ATTR_SBOOT_DISABLE                  0x00
#define ATTR_SBOOT_ENABLE                   0x11
#define ATTR_SBOOT_ONLY_ENABLE_ON_SCHIP     0x22
/* S-BOOT Control */
#define SEC_BOOT_CFG                        CUSTOM_SBOOT_CFG

/* Note : these attributes only work when S-BOOT is enabled */
#define VERIFY_PART_UBOOT                   (FALSE)
#define VERIFY_PART_LOGO                    (FALSE)
#define VERIFY_PART_BOOTIMG                 (FALSE)
#define VERIFY_PART_RECOVERY                (FALSE)
#define VERIFY_PART_ANDSYSIMG               (FALSE)
#define VERIFY_PART_SECSTATIC               (FALSE)

/**************************************************************************
 * [DEFINITION CHECK]
 **************************************************************************/
#if SEC_USBDL_CFG
#if !SEC_ENV_ENABLE
#error "SEC_USBDL_CFG is NOT disabled. Should set SEC_ENV_ENABLE as TRUE"
#endif
#endif

#if SEC_BOOT_CFG
#if !SEC_ENV_ENABLE
#error "SEC_BOOT_CFG is NOT disabled. Should set SEC_ENV_ENABLE as TRUE"
#endif
#endif

#endif   /* CUST_SEC_CTRL_H */
