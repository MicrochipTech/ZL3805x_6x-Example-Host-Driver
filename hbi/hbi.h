/*
* hbi.h  --  Header file for Microsemi ZL380xx Device HBI access
*
*/
/*******************************************************************************
* Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

#ifndef __HBI_H__
#define __HBI_H__
#include <stdbool.h>

/*! \brief enumerates various status codes of HBI Driver
 *
 */
typedef enum
{
    HBI_STATUS_NOT_INIT,     /*!<  driver not initilised */
    HBI_STATUS_INTERNAL_ERR, /*!< platform specific layer reported an error */
    HBI_STATUS_RESOURCE_ERR, /*!< request resource unavailable */
    HBI_STATUS_INVALID_ARG,  /*!< invalid argument passed to a function call */
    HBI_STATUS_BAD_HANDLE,   /*!< a bad reference handle passed */
    HBI_STATUS_BAD_IMAGE,    /*!< requested firmware image not present on flash */
    HBI_STATUS_FLASH_FULL,   /*!< no more space left on flash */
    HBI_STATUS_NO_FLASH_PRESENT, /*!< no flash connected to device */
    HBI_STATUS_COMMAND_ERR,    /*!< HBI Command failed */
    HBI_STATUS_INCOMPAT_APP,   /*!< firmware image is incompatible */
    HBI_STATUS_INVALID_STATE,  /*!< driver is in invalid state for current action */
    HBI_STATUS_OP_INCOMPLETE, /*!< operation incomplete */
    HBI_STATUS_SUCCESS         /*!< driver call successful */
}HbiStatus;

typedef enum
{
    HMI_RESP_SUCCESS = 0,
    HMI_RESP_BAD_IMAGE,
    HMI_RESP_CKSUM_FAIL,
    HMI_RESP_FLASH_FULL,
    HMI_RESP_CONF_REC_MISMATCH,
    HMI_RESP_INVALID_FLASH_HEAD,
    HMI_RESP_NO_FLASH_PRESENT,
    HMI_RESP_FLASH_FAILURE,
    HMI_RESP_COMMAND_ERROR,
    HMI_RESP_NO_CONFIG_RECORD,
    HMI_RESP_INV_CMD_APP_IS_RUNNING,
    HMI_RESP_INCOMPAT_APP,
    HMI_RESP_FLASH_INIT_NO_DEV = 0x0000,
    HMI_RESP_FLASH_INIT_UNRECOG_DEV = 0x8000,
    HMI_RESP_FLASH_INIT_OK = 0x6000,
    HMI_RESP_FLASH_INIT_BAD_CHKSUM_DEV = 0x0001
}ZL380xx_HMI_RESPONSE;

#define ZL380xx_CUR_FW_APP_RUNNING             (1<< 15)
#define HOST_CMD_HOST_FLASH_INIT               0x000B
#define HOST_CMD_ERASE_FLASH_INIT              0x09

#define HBI_DEV_ENDIAN_BIG                     0
#define HBI_DEV_ENDIAN_LITTLE                  1

/*Assuming host is little endian. user needs to set the host endianess here. */
#define HOST_ENDIAN_LITTLE                     1 

/* Macro that helps identifying host and device endian compatibility */
#if (HOST_ENDIAN_LITTLE)
#define MATCH_ENDIAN(devEndian) (devEndian == HBI_DEV_ENDIAN_LITTLE) 
#else 
#define MATCH_ENDIAN(devEndian) (devEndian == HBI_DEV_ENDIAN_BIG) 
#endif  

/* Macro that reformat values read/written to device in device format */
#define HBI_VAL(dev,val)  \
   (!MATCH_ENDIAN(dev) ? \
   (((val & 0xFF) << 8) | (val >>8)) : val)

/* Macro to check for HBI status okay or not */
#define CHK_STATUS(status)  \
   if (status != HBI_STATUS_SUCCESS) { \
           printf("ERROR %d: \n", status); \
           return status; \
                                     } 
bool HbiPortOpen(int32_t *fd);

void HbiPortClose(int32_t fd);

bool HbiPortWrite(int32_t fd, uint8_t const *tx, uint8_t const *rx, size_t len);

bool HbiPortRead(int32_t fd, void *pSrc, void *pDst, size_t nread, size_t nwrite);

HbiStatus HbiEraseFlash(int32_t fd);

HbiStatus HbiWrite(int32_t fd, uint16_t reg, uint8_t *data, int32_t size);

HbiStatus HbiRead(int32_t fd, uint16_t reg, uint8_t *buf, int32_t size);

HbiStatus HbiWriteHostCmd(int32_t fd, uint16_t cmd);

void HbiPortDelay(int32_t msec /*milliseconds*/);
#endif /* __HBI_H__*/