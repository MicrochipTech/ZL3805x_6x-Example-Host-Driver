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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "hbi.h"

#define MAX_HBI_BYTES_PER_ACCESS               256
#define HBI_BUFFER_SIZE                        256

/* TW firmware bin image header description */
/* header field width in bytes */
#define VER_WIDTH          1
#define FORMAT_WIDTH       1
#define OPN_WIDTH          2
#define BLOCK_SIZE_WIDTH   2
#define TOTAL_LEN_WIDTH    4
#define RESERVE_LEN_WIDTH  2
/* unused right now */
//#define FWR_CHKSUM_LEN     1
#define IMG_HDR_LEN    \
   (VER_WIDTH + FORMAT_WIDTH +  \
   OPN_WIDTH + BLOCK_SIZE_WIDTH + TOTAL_LEN_WIDTH + RESERVE_LEN_WIDTH)
/* field index */
#define VER_INDX        0
#define FORMAT_INDX     (VER_INDX+VER_WIDTH)
#define FWR_OPN_INDX    (FORMAT_INDX+FORMAT_WIDTH)
#define BLOCK_SIZE_INDX (FWR_OPN_INDX + OPN_WIDTH)
#define TOTAL_LEN_INDX  (BLOCK_SIZE_INDX + BLOCK_SIZE_WIDTH)
/* Image Version Info */
#define IMG_VERSION_MAJOR_SHIFT 6
#define IMG_VERSION_MINOR_SHIFT 4

/* image type fields */
#define IMG_HDR_TYPE_SHIFT    6
#define IMG_HDR_ENDIAN_SHIFT  5	

/* These tables are generated using twConvertFirmware2c.c file */
extern const unsigned char config[];
extern const unsigned char fwr[];

typedef enum
{
    HBI_IMG_TYPE_FWR = 0, /*!< firmware image to load on Device */
    HBI_IMG_TYPE_CR = 1,  /*!< Configuration Record to load Device */
    HBI_IMG_TYPE_LAST  /*!< Limiter on input type */
}hbi_img_type_t;


/*! \brief provide header information of firmware image passed by user
 *
 */
typedef struct
{
    int   major_ver;  /*!< header version major num */
    int   minor_ver;  /*!< header version minor num */
    hbi_img_type_t image_type; /*!< firmware or configuration record */
    int    endianness; /*!< endianness of the image excluding header */
    int    fwr_code;   /*!< if image_type == firmware, tells the firmware opn code */
    size_t block_size; /*!< block length in words image is divided into */
    size_t img_len;  /*!< total length of the image excluding header  */
    int    hdr_len;    /*!< length of header */
}hbi_img_hdr_t;

static unsigned char image[HBI_BUFFER_SIZE];

static inline HbiStatus twBootConclude(int32_t fd)
{
    uint16_t                val = 0;
    HbiStatus status = HBI_STATUS_SUCCESS;
    ZL380xx_HMI_RESPONSE hmi_response;

    /*HOST_CMD_HOST_LOAD_CMP*/
    status = HbiWriteHostCmd(fd, 0x000D); /*loading complete*/
    CHK_STATUS(status);

    /*Checks the status register to know result of command issued    */
    /*0x0034 Host Command Param/Result register*/
    status = HbiRead(fd, 0x0034, (uint8_t  *)&val, sizeof(val));
    hmi_response = (int32_t)val;
    printf("hmi_response status %d\n", hmi_response);
    switch (hmi_response)
    {
    case HMI_RESP_INCOMPAT_APP:
        return HBI_STATUS_INCOMPAT_APP;
    case HMI_RESP_SUCCESS:
        return HBI_STATUS_SUCCESS;
    }
    return HBI_STATUS_COMMAND_ERR;
}
static HbiStatus HbiResetToBoot(int32_t fd)
{
    HbiStatus status = HBI_STATUS_SUCCESS;
    uint16_t     val = 0;

    val = 1;
    status = HbiWrite(fd, 0x014, (uint8_t *)&val, 2); /*go to boot rom mode*/

    /*required for the reset to cmplete. */
    HbiPortDelay(50);

    /*check whether the device has gone into boot mode as ordered*/
    status = HbiRead(fd, 0x0034, (uint8_t *)&val, 2);
    if (status != HBI_STATUS_SUCCESS)
    {
        printf("HBI Read Failed \n");
        return HBI_STATUS_INTERNAL_ERR;
    }

    if ((val != 0xD3D3))
    {
        printf("ERROR: HBI is not accessible, cmdResultCheck = 0x%04x\n", val);
        status = HBI_STATUS_INTERNAL_ERR;
    }
    return status;

}
HbiStatus HbiSwitchToBootMode(int32_t fd)
{
    uint16_t      val1 = 0;
    HbiStatus  status;
    int32_t       boot_stat = 1;
    /* read the app running status bit from reg 0x28 "Currently Loaded Firmware Reg" */
    status = HbiRead(fd, 0x0028, (uint8_t *)&val1, sizeof(val1));
    //val1 = HBI_VAL(HBI_DEV_ENDIAN_BIG, val1);
    if (val1 & ZL380xx_CUR_FW_APP_RUNNING)
        boot_stat = 0;
    if (!boot_stat)
    {
        /* put device in boot rom mode */
        status = HbiResetToBoot(fd);
    }
    return status;
}
static HbiStatus twBootWrite(int32_t fd, void *pData, int32_t size)
{
    HbiStatus        status = HBI_STATUS_SUCCESS;
    unsigned char       *pFwrData;
    size_t               len;
    int32_t              ret;
    int32_t 				stat = 0;
    uint16_t                val = 0;
    int32_t                 boot_stat = 1;
    pFwrData = (unsigned char *)(pData);
    len = size;

    status = HbiSwitchToBootMode(fd);
    CHK_STATUS(status);

    stat = HbiPortWrite(fd, pFwrData, NULL, len);
    if (stat < 0)
    {
        status = HBI_STATUS_INTERNAL_ERR;
    }

    return status;
}
static inline HbiStatus  twStartFwrFromRam(int fd)
{
    int32_t  ret;
    HbiStatus status = HBI_STATUS_SUCCESS;
    ZL380xx_HMI_RESPONSE    hmi_response;
    uint16_t                val = 0;

    status = HbiSwitchToBootMode(fd);
    CHK_STATUS(status);

    status = HbiWriteHostCmd(fd, 0x08/*HOST_CMD_FWR_GO*/);
    CHK_STATUS(status);

    /*Checks the status register to know result of command issued    */
    /*0x0034 Host Command Param/Result register*/
    status = HbiRead(fd, 0x0034, (uint8_t  *)&val, sizeof(val));
    hmi_response = (int32_t)val;
    if (hmi_response != HMI_RESP_SUCCESS)
    {
        printf("Command failed with response 0x%x\n", hmi_response);
        return HBI_STATUS_COMMAND_ERR;
    }
    return status;
}

static HbiStatus twConfigWrite(int32_t fd, void *pData, int32_t size)
{
    HbiStatus        status = HBI_STATUS_SUCCESS;
    unsigned char       *pCfgData;
    size_t               len;
    int 					ret = 0;

    pCfgData = (unsigned char *)(pData);
    len = size;

    ret = HbiPortWrite(fd, pCfgData, NULL, len);
    if (ret < 1)
    {
        status = HBI_STATUS_INTERNAL_ERR;
    }

    return status;
}
static inline HbiStatus twSaveFwrcfgToFlash(int32_t fd,
    void *pVal)
{
    HbiStatus               status;
    uint16_t                num_fwr_images = 0;
    ZL380xx_HMI_RESPONSE    hmi_response;
    int32_t                 ret;
    uint16_t                val;

    status = HbiSwitchToBootMode(fd);
    CHK_STATUS(status);

    /* if there is a flash on board initialize it HOST_CMD_HOST_FLASH_INIT */
    status = HbiWriteHostCmd(fd, HOST_CMD_HOST_FLASH_INIT);
    CHK_STATUS(status);

    /*Checks the status register to know result of command issued    */
    /*0x0034 Host Command Param/Result register*/
    status = HbiRead(fd, 0x0034, (uint8_t  *)&val, sizeof(val));
    hmi_response = (int32_t)val;
    switch (hmi_response)
    {
    case HMI_RESP_FLASH_INIT_OK:
        status = HBI_STATUS_SUCCESS;
        break;
    case HMI_RESP_FLASH_INIT_NO_DEV:
        return HBI_STATUS_NO_FLASH_PRESENT;
    default:
        return HBI_STATUS_INTERNAL_ERR;
    }

    val = 0;
    /* 0x01F2 - ZL380xx_CFG_REC_CHKSUM_REG*/
    status = HbiWrite(fd, 0x01F2, (uint8_t  *)&val, 2);
    if (status != HBI_STATUS_SUCCESS)
    {
        printf("ERROR %d: \n", status);
        return status;
    }

    /*save the image to flash*/
    status = HbiWriteHostCmd(fd, 0x04);
    CHK_STATUS(status);

    /*Checks the status register to know result of command issued    */
    /*0x0034 Host Command Param/Result register*/
    status = HbiRead(fd, 0x0034, (uint8_t  *)&val, sizeof(val));
    hmi_response = (int32_t)val;

    if (hmi_response != HMI_RESP_SUCCESS)
    {
        printf("Command Result 0x%x \n", hmi_response);
        if (hmi_response == HMI_RESP_FLASH_FULL)
        {
            printf("Please erase flash to free up space\n");
            return HBI_STATUS_FLASH_FULL;
        }
        return HBI_STATUS_COMMAND_ERR;
    }
    if (pVal)
    {
        /* 0x0026 ZL380xx_FWR_COUNT_REG*/
        status = HbiRead(fd, 0x0026, (uint8_t *)&num_fwr_images, 2);
        CHK_STATUS(status);

        *(int *)pVal = num_fwr_images;
        printf("no of flash in the device %d\n", *(int *)pVal);
    }
    return status;

}
HbiStatus getHeader(unsigned char *pData, hbi_img_hdr_t *pHdr)
{


    if (pData == NULL || pHdr == NULL)
    {
        printf("Invalid arguments passed\n:");
        return HBI_STATUS_INVALID_ARG;
    }

    pHdr->major_ver = pData[VER_INDX] >> IMG_VERSION_MAJOR_SHIFT;
    pHdr->minor_ver = pData[VER_INDX] >> IMG_VERSION_MINOR_SHIFT;
    pHdr->image_type = pData[FORMAT_INDX] >> IMG_HDR_TYPE_SHIFT;

    pHdr->endianness = pData[FORMAT_INDX] >> IMG_HDR_ENDIAN_SHIFT;
    pHdr->fwr_code = (pData[FWR_OPN_INDX] << 8) | pData[FWR_OPN_INDX + 1];

    pHdr->block_size = (pData[BLOCK_SIZE_INDX] << 8) | pData[BLOCK_SIZE_INDX + 1];

    pHdr->img_len = pData[TOTAL_LEN_INDX] << 24;
    pHdr->img_len |= pData[TOTAL_LEN_INDX + 1] << 16;
    pHdr->img_len |= pData[TOTAL_LEN_INDX + 2] << 8;
    pHdr->img_len |= pData[TOTAL_LEN_INDX + 3];

    pHdr->hdr_len = IMG_HDR_LEN;

    return HBI_STATUS_SUCCESS;
}

HbiStatus vprocLoadImage(int32_t fd, const unsigned char *loadPtr) {

    HbiStatus   status = HBI_STATUS_SUCCESS;
    size_t         len;
    int            i;
    int             c, dataLen;
    unsigned char  *pData;
    uint32_t        block_size, data_size;
    hbi_img_hdr_t   hdr;
    size_t          fwr_len;

    /* init to null on safer side*/
    pData = image;

    for (i = 0; i < HBI_BUFFER_SIZE; i++)
    {
        pData[i] = loadPtr[i];
    }
    /*Firmware image is organised into chunks of fixed length and this information
      is embedded in image header. Thus first read image header and
      then start reading chunks and loading on to device
      */
    status = getHeader(pData, &hdr);
    if (status != HBI_STATUS_SUCCESS)
    {
        printf("HBI_get_header() err 0x%x \n", status);
        return status;
    }

    /* length is in unit of 16-bit words */
    block_size = (hdr.block_size) * 2;
    data_size = block_size;
    fwr_len = hdr.img_len;

    if (block_size > HBI_BUFFER_SIZE)
    {
        printf("Insufficient buffer size. please recompiled with increased HBI_BUFFER_SIZE\n");
        return HBI_STATUS_RESOURCE_ERR;
    }

    printf("\nSending image data ...\n");

    /* skip header from file.
       Re-adjust file pointer to start of actual data.
       */
    len = 0;
    dataLen = hdr.hdr_len;

    while (len < fwr_len)
    {
        for (i = 0; i < block_size; i++)
        {
            pData[i] = loadPtr[dataLen + i];
        }
        dataLen += block_size;
        len += block_size;


        if (hdr.image_type == HBI_IMG_TYPE_FWR)
        {
            status = twBootWrite(fd, pData, data_size);/* HBI_CMD_LOAD_FWR_FROM_HOST */
        }
        else if (hdr.image_type == HBI_IMG_TYPE_CR)
        {
            status = twConfigWrite(fd, pData, data_size); /*HBI_CMD_LOAD_CFGREC_FROM_HOST */
        }
        else {
            printf("Error %d:Unrecognized image type %d\n", status, hdr.image_type);
            status = HBI_STATUS_INVALID_ARG;
        }
        if (status != HBI_STATUS_SUCCESS)
        {
            printf("Error %d:HBI_set_command(HBI_CMD_LOAD_FWR_FROM_HOST)\n", status);
            return status;
        }
    }

    if (hdr.image_type == HBI_IMG_TYPE_FWR)
    {
        status = twBootConclude(fd);
        if (status != HBI_STATUS_SUCCESS) {
            printf("Error 1 %d:HBI_set_command(HBI_CMD_START_FWR)\n", status);
            return status;
        }
    }
    printf("Image loaded into Device\n");

    return HBI_STATUS_SUCCESS;
}

void main(void)
{
    HbiStatus  status;
    int  bSaveToFlash = 1; /* set it to zero to skip save to flash functionality*/
    int c, i;
    int fwrLoaded = 0, cfgrecLoaded = 0;
    int imageNum;
    int ret;
    const unsigned char * fwrAddress = &fwr[0];
    const unsigned char * configAddress = &config[0];
    int fd;

    ret = HbiPortOpen(&fd);

    status = vprocLoadImage(fd, fwrAddress);
    if (status == HBI_STATUS_SUCCESS)
    {
        fwrLoaded = 1;
    }
    else
    {
        printf("Error loading firmware\n");
        HbiPortClose(fd);
        return;
    }

    printf("Loading Configuration Record...\n");

    status = vprocLoadImage(fd, configAddress);
    if (status == HBI_STATUS_SUCCESS)
    {
        printf("Config Loading Done.\n");
        cfgrecLoaded = 1;
    }
    else
    {
        printf("Error loading config record\n");
    }

    if (fwrLoaded)
    {

        if (bSaveToFlash)
        {
            printf("\nSaving Firmware and Configuration Record to flash\n");

            status = twSaveFwrcfgToFlash(fd, &imageNum);
            if (status != HBI_STATUS_SUCCESS)
            {
                printf("Error %d:HBI_set_command(HBI_CMD_SAVE_FWRCFG_TO_FLASH)\n", status);
            }
            else
            {
                printf("Image %d saved to flash \n", imageNum);
            }
        }
        printf("\nStart Firmware\n");

        status = twStartFwrFromRam(fd);
        if (status != HBI_STATUS_SUCCESS)
        {
            printf("Error %d:HBI_set_command(HBI_CMD_START_FWR)\n", status);
        }

    }
    fwrLoaded = 0;
    cfgrecLoaded = 0;
    printf("Closing device file....\n");
    HbiPortClose(fd);
    return;
}


/** \} */

