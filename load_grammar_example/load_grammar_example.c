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
/* ------------------------------------------------------------ */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "hbi.h"

/* size of buffer for HBI read/write*/
#define MAX_RW_SIZE 64

/* These tables are generated using twConvertFirmware2c.c file */
extern const unsigned char grammar[];
extern const unsigned int grammar_size;

/* ------------------------------------------------------------ */
void BusySpinWait(int32_t fd)
{
    uint16_t buf = 0xFFFF;
    while (buf == 0xFFFF)
    {
        HbiRead(fd, 0x032, (uint8_t *)&buf, 2);
    }
}
unsigned int Buffer2Int(unsigned char  *pData)
{
    unsigned int integer;

    integer = (unsigned int)pData[1];
    integer = (integer << 8) + (unsigned int)pData[0];
    integer = (integer << 8) + (unsigned int)pData[3];
    integer = (integer << 8) + (unsigned int)pData[2];

    return integer;
}
/* ------------------------------------------------------------ */
void Int2Buffer(unsigned int integer, unsigned char *pData)
{

    pData[0] = (unsigned char)((integer >> 16) & 0x000000FF);
    pData[1] = (unsigned char)((integer >> 24) & 0x000000FF);
    pData[2] = (unsigned char)((integer)& 0x000000FF);
    pData[3] = (unsigned char)((integer >> 8) & 0x000000FF);
}

void LoadGrammarFile(int32_t fd, unsigned char * grammarPtr)
{
    size_t byteCount;
    unsigned char  buf[MAX_RW_SIZE];
    HbiStatus status;
    unsigned char offset, segAddress[4], segAddressTemp[4], segSize[4], AddWr[4];
    unsigned short lastSegIndex;
    unsigned int maxSize;
    int i;
    unsigned int len = 0;
    uint16_t val;

    /* Read the ASR segment address */
    HbiRead(fd, 0x4B8, segAddress, 4);
    offset = segAddress[2];


    /* Read the ASR max address */
    HbiRead(fd, 0x4BC, segAddressTemp, 4);

    /* Get the grammar max size */
    maxSize = Buffer2Int(segAddressTemp) - Buffer2Int(segAddress) - 1;

    if (grammar_size > maxSize) {
        printf("Error - LoadGrammarFile(): Grammar file to large (exceeds %d bytes)\n", maxSize);
        HbiPortClose(fd);
        exit(-1);
    }

    /* Store the start address for later use */
    memcpy(segAddressTemp, segAddress, 4);

    /* Disable the ASR */
    val = 0x800D;
    HbiWrite(fd, 0x032, (uint8_t *)&val, 2);
    val = 4;
    HbiWrite(fd, 0x006, (uint8_t *)&val, 2);
    BusySpinWait(fd);

    byteCount = MAX_RW_SIZE;
    while (len < grammar_size)
    {
        if ((grammar_size - len) < MAX_RW_SIZE)
        {
            byteCount = (grammar_size - len);
        }
        /* Read a block of data from the bin file */
        for (i = 0; i < byteCount; i += 2)
        {
            /* reading data in this order since we have the endianess conversion in HbiWrite function*/
            buf[i] = grammarPtr[len + i + 1];
            buf[i + 1] = grammarPtr[len + i];
        }
        len += byteCount;

        /* Store the start address for later use */
        memcpy(AddWr, segAddress, 4);

        /* Write the address for a page 255 type access */
        HbiWrite(fd, 0x00C, AddWr, 4);


        /* Write the block */
        HbiWrite(fd, 0xFF00 | (uint16_t)offset, buf, byteCount);


        Int2Buffer(Buffer2Int(segAddress) + byteCount, segAddress);

        offset = segAddress[2];

    }

    /* Update the segment table */
    /* Recover the start address */
    memcpy(segAddress, segAddressTemp, 4);

    HbiRead(fd, 0x13E, (uint8_t *)&val, 2);
    /* get the number of segments */
    lastSegIndex = val - 1;

    /* Read the load address of the last segment */
    HbiRead(fd, 0x144 + 8 * lastSegIndex, segAddressTemp, 4);

    /* Convert the grammar size in a buffer */
    Int2Buffer(grammar_size, segSize);

    /* If the last segment address is an ASR segment, update the size otherwise create a new segment */
    if (Buffer2Int(segAddressTemp) == Buffer2Int(segAddress))
    {
        /* Update the last segment size */
        HbiWrite(fd, 0x140 + 8 * lastSegIndex, segSize, 4);
    }
    else
    {
        /* Create a new segment */
        lastSegIndex++;
        HbiWrite(fd, 0x140 + 8 * lastSegIndex, segSize, 4);
        HbiWrite(fd, 0x144 + 8 * lastSegIndex, segAddress, 4);
        val = lastSegIndex + 1;
        HbiWrite(fd, 0x13E, (uint8_t *)&val, 2);
    }

    /* Enable the ASR */
    val = 0x800E;
    HbiWrite(fd, 0x032, (uint8_t *)&val, 2);
    val = 4;
    HbiWrite(fd, 0x006, (uint8_t *)&val, 2);
    BusySpinWait(fd);

    printf("Info - Grammar successfully loaded to RAM\n");
}


int main(int argc, char** argv)
{
    int c, ret, numGrammars, queryNumGrammar = 0, saveToFlash = 0, grammarIdx = -1;
    char *binGrammarPath = NULL;
    HbiStatus  status;
    int fd;
    uint16_t     val;
    const unsigned char * grammarPtr = &grammar[0];

    ret = HbiPortOpen(&fd);
    /* The firmware needs to be running in order to manage grammars */

    HbiRead(fd, 0x028, (uint8_t *)&val, 2);
    if ((val & 0x8000) == 0) {
        printf("Error - Main(): Application firmware stopped\n");
        HbiPortClose(fd);
        exit(-1);
    }

    LoadGrammarFile(fd, grammarPtr);

    /* Close the HBI driver */
    HbiPortClose(fd);
    return 0;
}