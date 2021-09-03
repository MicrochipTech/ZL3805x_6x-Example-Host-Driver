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
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <time.h>
#include "hbi.h"
/**************************************************************/
/* 			Hbi_frame_hdr() macro definition  				  */
/**************************************************************/

#define HBI_PAGED_READ(offset,length) \
    ((uint16_t)(((uint16_t)(offset) << 8) | (length)))
#define HBI_DIRECT_READ(offset,length) \
    ((uint16_t)(0x8000 | ((uint16_t)(offset) << 8) | (length)))
#define HBI_PAGED_WRITE(offset,length) \
    ((uint16_t)(HBI_PAGED_READ(offset,length) | 0x0080))
#define HBI_DIRECT_WRITE(offset,length) \
    ((uint16_t)(HBI_DIRECT_READ(offset,length) | 0x0080))
#define HBI_SELECT_PAGE(page) \
    ((uint16_t)(0xFE00 | (page)))	

#define ZL380xx_MAX_ACCESS_SIZE_IN_BYTES       256 /*128 16-bit words*/
#define TWOLF_MBCMDREG_SPINWAIT                10000
#define ZL380xx_HOST_SW_FLAGS_HOST_CMD         1

static const char *device = "/dev/spidev0.0";
static uint32_t mode;
static uint8_t  bits = 8;
static uint32_t speed = 20000000;//500000;

/********************************************************************/
/* 	Open SPI device				 		                            */
/*	initialise SPI mode, bits. speed etc                            */
/* This example implementation is for user-mode linux. This 	    */
/* code should be ported to the client's specific HW host mcu/mpu   */
/********************************************************************/

bool HbiPortOpen(int32_t *fd)
{
    int32_t ret;
    int32_t handle = *fd;
    handle = open(device, O_RDWR);
    if (handle < 0)
    {
        printf("can't open device");
        return false;
    }

    /* 					 					 */
    /*	initialise SPI mode, bits. speed etc */

    /*
    * spi mode
    */
    ret = ioctl(handle, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
    {
        printf("can't set spi mode");
        return false;
    }

    ret = ioctl(handle, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
    {
        printf("can't get spi mode");
        return false;
    }
    /*
    * bits per word
    */
    ret = ioctl(handle, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        printf("can't set bits per word");
        return false;
    }
    ret = ioctl(handle, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
    {
        printf("can't get bits per word");
        return false;
    }
    /*
    * max speed hz
    */
    ret = ioctl(handle, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        printf("can't set max speed hz");
        return false;
    }

    ret = ioctl(handle, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
    {
        printf("can't get max speed hz");
        return false;
    }
    printf("spi mode: 0x%x\n", mode);
    printf("bits per word: %u\n", bits);
    printf("max speed: %u Hz (%u kHz)\n", (speed), (speed) / 1000);

    *fd = handle;
    return true;

}
void HbiPortClose(int32_t fd)
{
    close(fd);
}
/*********************************************************************************/
/* 					Read from Device.							                 */
/* This example implementation is for user-mode linux. This 	                 */
/* code should be ported to the client's specific HW host mcu/mpu                */
/*********************************************************************************/
bool HbiPortRead(int32_t fd, void *pSrc, void *pDst, size_t nread, size_t nwrite)
{
    int32_t ret = 0;
    struct spi_ioc_transfer xfer[2] = { 0 };

    xfer[0].tx_buf = (unsigned long)pSrc;
    xfer[0].len = nwrite;
    xfer[0].speed_hz = speed;
    xfer[0].bits_per_word = bits;

    xfer[1].rx_buf = (unsigned long)pDst;
    xfer[1].len = nread;
    xfer[1].speed_hz = speed;
    xfer[1].bits_per_word = bits;

    ret = ioctl(fd, SPI_IOC_MESSAGE(2), &xfer);
    if (ret < 1)
    {
        printf("can't send spi message");
        return false;
    }

    return true;
}
/*********************************************************************************/
/* 					Write to Device.							                 */
/* This example implementation is for user-mode linux. This 	                 */
/* code should be ported to the client's specific HW host mcu/mpu                */
/*********************************************************************************/

bool HbiPortWrite(int32_t fd, uint8_t const *tx, uint8_t const *rx, size_t len)
{
    int32_t ret = 0;
    struct spi_ioc_transfer tr = { 0 };

    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx,
        tr.len = len;
    tr.speed_hz = speed;
    tr.bits_per_word = bits;

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
    {
        printf("hbi_spi_write: can't send spi message");
        return false;
    }

    return true;
}
/* delay function */
void HbiPortDelay(int32_t msec /*milliseconds*/)
{
    long delayT;
    clock_t start, end;

    delayT = msec*(CLOCKS_PER_SEC / 1000);
    start = end = clock();
    while ((end - start) < delayT)
    {
        end = clock();
    }
}
/*********************************************************************************/
/*  Description: this function makes transport frame header for command          */
/*  to be sent over HBI                                                          */
/*********************************************************************************/
static void HbiFrameHdr(uint16_t 	addr,
    int32_t       read,
    size_t     size,
    uint8_t 	*cmd,
    size_t		*cmdlen)
{
    uint8_t         page = addr >> 8;
    uint8_t         offset = (addr & 0xFF) >> 1;
    uint8_t         num_words = (size >> 1) - 1;
    uint16_t        val = 0;
    int32_t         i = 0;

    *cmdlen = 0;
    if (page)
    {
        if (page != 0xFF)
        {
            page -= 1;
        }

        val = HBI_SELECT_PAGE(page);

        i = 0;
        cmd[i++] = val >> 8;
        cmd[i++] = val & 0xFF;

        if (read)
        {
            val = HBI_PAGED_READ(offset, num_words);
        }
        else
        {
            val = HBI_PAGED_WRITE(offset, num_words);
        }

        cmd[i++] = val >> 8;
        cmd[i++] = val & 0xFF;
    }
    else
    {
        /*Direct page access. Make read or write command*/
        if (read)
        {
            val = HBI_DIRECT_READ(offset, num_words);
        }
        else
        {
            val = HBI_DIRECT_WRITE(offset, num_words);
        }

        i = 0;

        cmd[i++] = val >> 8;
        cmd[i++] = val & 0xFF;
    }

    *cmdlen = i;

    return;
}

HbiStatus HbiWrite(int32_t fd, uint16_t reg_addr, uint8_t *data, int32_t size)
{
    uint8_t cmd[4] = { 0 };
    uint8_t buf[260]; //buffer size max will be 256+4 (data size + command size) 
    size_t cmd_len, i;
    int32_t ret = 0;
    uint16_t *dataPtr = (uint16_t *)data;
    HbiStatus status = HBI_STATUS_SUCCESS;
    if (size > ZL380xx_MAX_ACCESS_SIZE_IN_BYTES)
    {
        printf("Size exceed.Received %d,Maximum limit to transfer data is %d\n",
            (int32_t)size, (int32_t)ZL380xx_MAX_ACCESS_SIZE_IN_BYTES);
        return HBI_STATUS_INVALID_ARG;
    }

    for (i = 0; i < (size >> 1); i++)
    {
        dataPtr[i] = HBI_VAL(HBI_DEV_ENDIAN_BIG, dataPtr[i]);
    }

    HbiFrameHdr(reg_addr, 0, size, &cmd[0], &cmd_len);
    for (i = 0; i < cmd_len; i++)
    {
        buf[i] = cmd[i];
    }
    for (i = 0; i < size; i++)
    {
        buf[cmd_len + i] = data[i];
    }


    ret = HbiPortWrite(fd, &buf[0], NULL, cmd_len + size);

    if (ret < 1)
    {
        status = HBI_STATUS_INTERNAL_ERR;
        return status;
    }

    return status;
}


HbiStatus HbiRead(int32_t fd, uint16_t reg_addr, uint8_t *buf, int32_t size)
{
    uint8_t cmd[4] = { 0 };
    size_t cmd_len, i;
    int32_t ret = 0;
    uint16_t *bufPtr = (uint16_t *)buf;
    uint16_t temp;
    HbiStatus status = HBI_STATUS_SUCCESS;

    HbiFrameHdr(reg_addr, 1, size, &cmd[0], &cmd_len);

    ret = HbiPortRead(fd, &cmd, buf, size, cmd_len);

    if (ret < 1)
    {
        status = HBI_STATUS_INTERNAL_ERR;
        return status;
    }
    for (i = 0; i < (size >> 1); i++)
    {
        temp = bufPtr[i];
        bufPtr[i] = HBI_VAL(HBI_DEV_ENDIAN_BIG, temp);
    }

    return status;
}

/*********************************************************************************/
/*    Writing a host command into Command register and processing it             */
/*     is a 3-step process                                                       */
/*     1. check if there's any command in process by monitoring sw flag regs.    */
/*     if yes, wait for it to compelete                                          */
/*     2. if no command in progress, then write current command in to command    */
/*     register and issue notice to firmware that a host command is written      */
/*     3. wait for current command to complete                                   */
/*********************************************************************************/
HbiStatus HbiWriteHostCmd(int32_t fd, uint16_t cmd)
{
    HbiStatus status = HBI_STATUS_SUCCESS;
    int32_t hmi_response;
    uint16_t  i = 0;
    uint16_t  temp = 0x0BAD;
    uint16_t  buf;
    /* check whether there's any ongoing command */
    for (i = 0; i < TWOLF_MBCMDREG_SPINWAIT; i++)
    {
        status = HbiRead(fd, 0x0006, (uint8_t *)&temp, sizeof(temp));
        CHK_STATUS(status);
        //temp = HBI_VAL(HBI_DEV_ENDIAN_BIG, temp);
        if (!(temp & 0x1))
        {
            break;
        }
        HbiPortDelay(10);
    }
    if ((i >= TWOLF_MBCMDREG_SPINWAIT) && (temp & 0x1))
    {
        return HBI_STATUS_RESOURCE_ERR;
    }

    /* write the command into the Host Command register*/
    /*0x0032:  Host Command register*/
    //cmd = HBI_VAL(HBI_DEV_ENDIAN_BIG, cmd);
    status = HbiWrite(fd, 0x0032, (uint8_t *)&cmd, sizeof(cmd));
    CHK_STATUS(status);

    /* Issue "Host Command Written" notice to firmware */
    cmd = 0x1;//HBI_VAL(HBI_DEV_ENDIAN_BIG, 0x1);
    status = HbiWrite(fd, 0x0006, (uint8_t *)&cmd, sizeof(cmd));
    CHK_STATUS(status);

    /* check whether the last command is completed */
    temp = 0x0BAD;
    for (i = 0; i < TWOLF_MBCMDREG_SPINWAIT; i++)
    {
        status = HbiRead(fd, 0x0032, (uint8_t *)&temp, sizeof(temp));
        CHK_STATUS(status);

        if (temp == 0) /* HOST_CMD_IDLE */
        {
            break;
        }
        HbiPortDelay(10);
    }
    if ((i >= TWOLF_MBCMDREG_SPINWAIT) && (temp != 0))
    {
        return HBI_STATUS_RESOURCE_ERR;
    }


    return status;
}




