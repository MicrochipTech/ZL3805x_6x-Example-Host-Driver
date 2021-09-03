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
#include "hbi.h"

int fd;
static inline HbiStatus HbiResetToBoot(int32_t fd)
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
HbiStatus HbiEraseFlash(int32_t fd)
{
    HbiStatus            status = HBI_STATUS_SUCCESS;
    uint16_t                val = 0;
    int32_t                 boot_stat = 1;
    ZL380xx_HMI_RESPONSE    hmi_response;

    /* read the app running status bit from reg 0x28 "Currently Loaded Firmware Reg" */
    status = HbiRead(fd, 0x0028, (uint8_t *)&val, sizeof(val));
    if (val & ZL380xx_CUR_FW_APP_RUNNING)
    {
        boot_stat = 0;
    }
    if (!boot_stat)
    {
        /* put device in boot rom mode */
        status = HbiResetToBoot(fd);
        CHK_STATUS(status);
    }

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
    /* erase all config/fwr */
    val = 0xAA55;
    status = HbiWrite(fd, 0x0034, (uint8_t *)&val, sizeof(val));
    CHK_STATUS(status);

    /* erase firmware */
    status = HbiWriteHostCmd(fd, HOST_CMD_ERASE_FLASH_INIT);
    CHK_STATUS(status);

    /*Checks the status register to know result of command issued    */
    /*0x0034 Host Command Param/Result register*/
    status = HbiRead(fd, 0x0034, (uint8_t  *)&val, sizeof(val));
    hmi_response = (int32_t)val;
    switch (hmi_response)
    {
    case HMI_RESP_FLASH_INIT_OK:
        return HBI_STATUS_SUCCESS;
    case HMI_RESP_BAD_IMAGE:
        return HBI_STATUS_BAD_IMAGE;
    case HMI_RESP_INCOMPAT_APP:
        return HBI_STATUS_INCOMPAT_APP;
    case HMI_RESP_NO_FLASH_PRESENT:
        return HBI_STATUS_NO_FLASH_PRESENT;
    default:
        printf("Command response 0x%x\n", hmi_response);
        return HBI_STATUS_COMMAND_ERR;
    }

}
int main()
{
    uint8_t rBuf[2] = { 0 };
    uint8_t wBuf[2] = { 0 };
    int ret;
    HbiStatus status;

    ret = HbiPortOpen(&fd);
    if (ret == 0)
    {
        printf("HbiPortOpen ERROR\n");
        return -1;
    }
    /********************************************/
    /*    Example for write register 0x000E      */
    /********************************************/
    wBuf[1] = 0xAB; wBuf[0] = 0xCD;
    status = HbiWrite(fd, 0x000E, &wBuf[0], 2);
    if (status != HBI_STATUS_SUCCESS)
    {
        printf("HBI write failed\n");
        HbiPortClose(fd);
        return -1;
    }

    /********************************************/
    /*    Example for read register 0x000E      */
    /********************************************/

    status = HbiRead(fd, 0x000E, &rBuf[0], 2);
    if (status != HBI_STATUS_SUCCESS)
    {
        printf("HBI read failed\n");
        HbiPortClose(fd);
        return -1;
    }

    printf("Register 0x000E value is 0x%02X%02X\n", rBuf[1], rBuf[0]);


    /*********************************************************************************************/
    /* This example shows how to do "ERASE FLASH"                                                */
    /*********************************************************************************************/

    status = HbiEraseFlash(fd);
    if (status == HBI_STATUS_SUCCESS)
    {
        printf("flash erasing completed successfully...\n");
    }
    else
    {
        printf("Error %d:HbiEraseFlash\n", status);
        HbiPortClose(fd);
        return -1;
    }
    HbiPortClose(fd);

    return 0;
}