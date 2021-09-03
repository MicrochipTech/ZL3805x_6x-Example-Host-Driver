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

/********************************************/
/*! \mainpage
*
* \section intro_sec Introduction
*
* This is an document summarizes Microsemi VPROC Device Image Convertor tool.
*
* \section BI Build Instructions
*
* Use any compiler on host machine.
*
* Example Make Command:
*
* gcc twConvertImage.c -o twConvertImage
*
* Usage:
*
* twConvertImage <options>
*
* Options:
*
* -i <firmware.s3/config record.cr2> - Input filename
*
* -o <output file name.c/.bin> - Converted output file name (.h or .bin)
*
* -f <firmware code> - Device code. Example
*                      if image or config record is for device zl38051, then -f 38051 must be
*                      mentioned.
*
* -b <block size in 16-bit word len> - Block size image to be divided to.
*                                      Size range for *.s3 is 16*2^n where n =(0, 1, 2, 3) .
*                                      Size range for *.cr2 is 1*2^n where n =(0, 1, 2, 3, 4, 5, 6, 7) .
*
* To Display help menu
*
* twConverImage -h
*
********************************************************************************/

#include <stdint.h>
#include <stdlib.h> /* malloc, free, rand */
#include <stdio.h>  /*getline(), etc...*/
#include <stdarg.h>
#include <time.h>       /* time_t, struct tm, time, localtime */
#include <string.h>
#include <unistd.h>

/* length in bytes */
#define HBI_MAX_PAGE_LEN   256

#define BUF_LEN         1024

#define VER_LEN_WIDTH      1
#define FORMAT_LEN_WIDTH   1
#define OPN_LEN_WIDTH      2
#define CHUNK_LEN_WIDTH    2
#define TOTAL_LEN_WIDTH    4
#define RESERVE_LEN_WIDTH  2

#define FWR_CHKSUM_LEN     1

#define IMG_HDR_LEN    \
   (VER_LEN_WIDTH +FORMAT_LEN_WIDTH +  \
   OPN_LEN_WIDTH + CHUNK_LEN_WIDTH + TOTAL_LEN_WIDTH + RESERVE_LEN_WIDTH)

/* Image Version Info */
#define IMG_VERSION_MAJOR_SHIFT 6
#define IMG_VERSION_MINOR_SHIFT 4
#define IMG_VERSION_MAJOR       0
#define IMG_VERSION_MINOR       0
#define IMG_HDR_VERSION \
         ((IMG_VERSION_MAJOR << IMG_VERSION_MAJOR_SHIFT) | \
          (IMG_VERSION_MINOR << IMG_VERSION_MINOR_SHIFT))

/* image type */
#define IMG_HDR_TYPE_SHIFT    6
#define IMG_HDR_TYPE(type)          (type<<IMG_HDR_TYPE_SHIFT) // 0 -fw, 1-cfg
#define IMG_HDR_ENDIAN_SHIFT  5
#define IMG_HDR_ENDIAN        (0<<IMG_HDR_ENDIAN_SHIFT) //0-big 1-little
//#define IMG_HDR_FORMAT        (IMG_HDR_TYPE | IMG_HDR_ENDIAN)
#define IMG_HDR_FORMAT(type)        ((IMG_HDR_TYPE(type)) | IMG_HDR_ENDIAN)

/* TW registers */
#define PAGE255_REG                 0x000C
#define HOST_FWR_EXEC_REG           0x012C /*Fwr EXEC register*/


typedef unsigned short u16;

/* HBI Commands */
#define HBI_CONFIGURE(pinConfig) \
            ((u16)(0xFD00 | (pinConfig)))

#define HBI_DIRECT_PAGE_ACCESS_CMD_LEN 2
#define HBI_DIRECT_PAGE_ACCESS_CMD 0x80

#define HBI_PAGE_WR_CMD_LOW_BYTE(length) ((0x80 | (length-1)))

#define HBI_SELECT_PAGE_CMD_LEN   2
#define HBI_SELECT_PAGE_CMD      0xFE
#define HBI_SELECT_PAGE(page)    ((u16)((HBI_SELECT_PAGE_CMD << 8) | page)))

#define HBI_PAGE_OFFSET_CMD_LEN        2
#define HBI_PAGED_OFFSET_MIN_DATA_LEN  2

#define HBI_CONT_PAGED_WR_CMD   0xFB 
#define HBI_CONT_PAGED_WRITE(length) ((u16)((HBI_CONT_PAGED_WR_CMD <<8) | length))

#define HBI_NO_OP_CMD 0xFF

//#define OUTBUF_NAME "buffer"

unsigned char outbuf[BUF_LEN];
unsigned short fw_opn_code;

/*config record structures*/
typedef struct {
    unsigned short integer_part;   /*the register */
    float decimal_part; /*the value to write into reg */
}splitfloat;

char *outpath, *inpath;
unsigned short len;
unsigned int total_len = 0;

FILE *saveFhande;
FILE *BOOT_FD;
unsigned short numElements;
int bOutputTypeC = 0;

/*config record structures*/
typedef struct {
    uint16_t reg;   /*the register */
    uint16_t value[128]; /*the value to write into reg */
} dataArr;

dataArr *pCr2Buf;

unsigned short  zl_firmwareBlockSize = 16;
unsigned short  zl_configBlockSize = 1;

#undef DISPLAY_TO_TERMINAL  /*to see the data while being converted*/
#define strupr(func) func

#undef DEBUG
#ifdef DEBUG
#define DBG printf
#else
#define DBG
#endif
void outLog(const char *str, ...);
void HbiPagedBootImage();
void dumpFile(unsigned char *buf, int len);
void readCfgFile();
/* Let's create our own lower case to UPPER case
 * Pass it a string of characters and it will return
 * that same strng in UPPER case character
 *
 * Note: c has an equivalent function named strupr, but it doesn't exist on Linux platform
 */
char * stringToUpperCase(char *sPtr)
{
    int i = 0;
    while (*(sPtr + i) != '\0')
    {
        *(sPtr + i) = toupper((unsigned char)*(sPtr + i));
        //DBG("%s\n", sPtr);
        i++;
    }
    return sPtr;
}


/* Let's create our own UPPER case to lower case
 * Pass it a string of characters and it will return
 * that same strng in lower case character
 *
 */
char * stringToLowerCase(char *sPtr)
{
    int i = 0;
    while (*(sPtr + i) != '\0')
    {
        *(sPtr + i) = tolower((unsigned char)*(sPtr + i));
        //DBG("%s\n", sPtr);
        i++;
    }
    return sPtr;
}

/*fast_log2(): Compute in floating the log base two of a number.
 *             The c math library includes a log2 but not only that library
 *             is big, but it is not available on every platform
 * Args:
 *       float val    ; the number for which to calculte the log2
 * Return:
 *       float        ; the computed log2 of val
 */
static float fast_log2(float val)
{
    if (val <= 0)
    {
        return (-1);
    }
    else
    {
        int *const exp_ptr = (int *)(&val); /* interpreting the float as a sequence of bits.*/
        int x = *exp_ptr;
        const int log_2 = ((x >> 23) & 255) - 128;

        x &= ~(255 << 23);
        x += 127 << 23;

        *exp_ptr = x;

        val = ((-1.0f / 3.0f) * val + 2) * val - 2.0f / 3.0f;
        return val + log_2;
    }
}

/*split_float(): split a float value into integer and decimal parts.
 * Args:
 *       float val    ; the number to split
 * Return:
 *       struct splitfloat    ; a structure containing the integer value and decimal value
 *                              of the splitted float
 */
static splitfloat split_float(float val) {
    splitfloat splitf;
    int intpart = (int)val;
    float decpart = val - intpart;
    splitf.integer_part = intpart;
    splitf.decimal_part = decpart;
    return splitf;
}

/*check_fwrblock_size(): this function verifies that the block size passed by
 *                       the user met the following requirements
 *                       block_size = 16*2^n where n = 0, or 1, or 2, or 3.
 * Args:
 *       int block_size    ; the user entered block size
 * Return:
 *       int      ; block_size if block size is correct, -1 if not correct
 */
static int check_fwrblock_size(int block_size)
{

    splitfloat splitf = split_float(fast_log2(block_size / 16));
    DBG("Val = %d, intpart = %d, decpart = %04f\n", block_size, splitf.integer_part, splitf.decimal_part);
    if ((block_size < 16) || (block_size > 128) || (splitf.integer_part > 3) || (splitf.decimal_part * 10 > 0))
    {
        return -1; //failure
    }
    return block_size;
}

/*check_cfgblock_size(): this function verifies that the block size passed by
 *                       the user met the following requirements
 *                       block_size = 1*2^n where n = 0, or 1, or 2, or 3, or 4
 *                                                         or 5, or 6 or 7
 * Args:
 *       int block_size    ; the user entered block size
 * Return:
 *       int      ; block_size if block size is correct, -1 if not correct
 */
static int check_cfgblock_size(int block_size)
{

    splitfloat splitf = split_float(fast_log2(block_size));
    int n = 1;
    DBG("Val = %d, intpart = %d, decpart = %04f\n", block_size, splitf.integer_part, splitf.decimal_part);
    if ((block_size < 1) || (block_size > 128) || (splitf.integer_part > 7) || (splitf.decimal_part * 10 > 0))
    {
        return -1; //failure
    }
    return block_size;
}
/* fseekNunlines() -- The firmware file is an ascii text file.
 * the information from fseek will not be usefull
 * this is our own fseek equivalent
 */
static unsigned long fseekNunlines(FILE *fptr) {
    unsigned short line_count = 0;
    int c;

    while ((c = fgetc(fptr)) != EOF)
    {
        if (c == '\n')
            line_count++;
    }
    len = line_count - 1;
    return len;
}

/*AsciiHexToHex() - to convert ascii char hex without leading 0x to integer hex
 * pram[in] - str - pointer to the char to convert.
 * pram[in] - len - the number of character to convert (2:u8, 4:u16, 8:u32).

 */
static unsigned int AsciiHexToHex(const char * str, unsigned char len)
{
    unsigned int val = 0;
    char c;
    unsigned char i = 0;
    for (i = 0; i < len; i++)
    {
        c = *str++;
        val <<= 4;

        if (c >= '0' && c <= '9')
        {
            val += c & 0x0F;
            continue;
        }

        c &= 0xDF;
        if (c >= 'A' && c <= 'F')
        {
            val += (c & 0x07) + 9;
            continue;
        }
        return 0;
    }
    return val;
}

/*AsciiHexToInt() - to convert ascii char hex with or without leading 0x to integer
 * pram[in] - str - pointer to the ascii-hex string  to convert.
 * pram[in] - len - the number of character to convert (2:u8, 4:u16, 8:u32).

 */
static int AsciiHexToInt(char *s, unsigned char len)
{
    int i = 0; /* Iterate over s */
    int n = 0; /* Built up number */

    /* Remove "0x" or "0X" */
    if (s[0] == '0' && s[1] == 'x' || s[1] == 'X')
        i = 2;

    while (i < len)
    {
        int t;

        if (s[i] >= 'A' && s[i] <= 'F')
            t = s[i] - 'A' + 10;
        else if (s[i] >= 'a' && s[i] <= 'f')
            t = s[i] - 'a' + 10;
        else if (s[i] >= '0' && s[i] <= '9')
            t = s[i] - '0';
        else
            return n;

        n = 16 * n + t;
        ++i;
    }

    return n;
}
/*If your C compiler include getline() then undefine APP_UTIL_USE_FGETS
 * Otherwise fgets() is used
 */
#define APP_UTIL_USE_FGETS
/* readCfgFile() use this function to
 * Read the Voice processing cr2 config file into a C code
 * filepath -- pointer to the location where to find the file
 * pCr2Buf -- the actual firmware data array will be pointed to this buffer
 */
void readCfgFile()
{
    uint16_t reg, val;
    int done = 0;
    int index = 0, j = 1;
    char s[2];
    unsigned int  byteCount = 0;
    int chars_per_entry = 6;
    int offset_to_shift = 0;
    int current_fpos = 0;
    uint16_t previous_reg = 0xFFFF;
    int k = 0;

    uint8_t *tracker = (uint8_t *)malloc(len*sizeof(uint8_t));

#ifdef APP_UTIL_USE_FGETS
    char line[1024] = "";
#else
    char  *line;
    size_t nread = 0;
    size_t bytes_read = 0;
#endif       
    int i = 0, n = 0;

    memset(tracker, 0, len);
    fseek(BOOT_FD, 0, SEEK_SET);
    /* allocate memory to contain the reg and val:*/
    pCr2Buf = (dataArr *)malloc(len*sizeof(dataArr));
    if (pCr2Buf == NULL) {
        printf("not enough memory to allocate %lu bytes.. ", len*sizeof(dataArr));
        return;
    }
    if (bOutputTypeC)
    {
        char *array_name = stringToUpperCase(strtok(outpath, "."));
        outLog("#ifndef __%s__\n", array_name);
        outLog("#define __%s__\n\n", array_name);

        outLog("const unsigned char %s[] ={\n", stringToLowerCase(array_name));
        current_fpos = ftell(saveFhande);
        /* Add 1 for newline char which is added in C- output */
        offset_to_shift = (IMG_HDR_LEN * chars_per_entry) + 1;
    }
    else
        offset_to_shift = IMG_HDR_LEN;

    fseek(saveFhande, offset_to_shift, SEEK_CUR);
    memset(outbuf, 0, sizeof(outbuf));

    /*read and format the data accordingly*/
    numElements = 0;
#ifdef APP_UTIL_USE_FGETS
    DBG("using fgets\n");
    while (fgets(line, 1024, BOOT_FD) != NULL)
#else
    DBG("using getline\n");
    while ((bytes_read = getline(&line, &nread, BOOT_FD)) != -1)
#endif
    {

        numElements++;
        if (line[0] != ';')
        {

            reg = AsciiHexToHex(&line[2], 4);
            val = AsciiHexToHex(&line[10], 4);

            //DBG("%s", line);
            if (i <= (zl_configBlockSize - 1))
            {
                if (index != j)
                {
                    pCr2Buf[index].reg = reg;
                    j = index;
                }
                /* check whether we are on the same page for that block
                 * we don't want to jump page within a block
                 * There is a hole in the config start a new block
                 */
                if (previous_reg == 0xFFFF)
                    previous_reg = reg;
                else {
                    //if ((reg >> 8) != (pCr2Buf[index].reg >> 8) && (i < (zl_configBlockSize -1)))
                    DBG("len = %d, numelements = %d\n", len, numElements);
                    DBG("index = %d, reg = 0x%04x  previous reg = 0x%04x\n\n", index, reg, previous_reg);
                    if (((reg - previous_reg) > 2) && (i < (zl_configBlockSize - 1)) ||
                        ((numElements > len) && (i < (zl_configBlockSize - 1))))
                    {
                        /*fill the hole with no-up*/
                        tracker[index] = i;
                        DBG("*****HOLE FOUND!!!******* filling hole, index = %d, i - n = %d - %d\n\n", index, i, zl_configBlockSize - 1);
                        for (k = i; k < zl_configBlockSize; k++)
                            pCr2Buf[index].value[k] = (HBI_NO_OP_CMD << 8) | HBI_NO_OP_CMD;

                        /*re-arm for new block*/
                        i = 0;
                        index++;
                        pCr2Buf[index].reg = reg;
                        j = index;
                    }
                }
                pCr2Buf[index].value[i] = val;

                DBG("index =%d: reg = 0x%04x : val = 0x%04x\n\n", index, pCr2Buf[index].reg, pCr2Buf[index].value[i]);
                if (i == (zl_configBlockSize - 1))
                {
                    i = 0;
                    index++;
                }
                else {
                    i++;
                }
            }
            previous_reg = reg;
        }
    }
    /*last check*/
    if ((i > 0) && (i < (zl_configBlockSize - 1)))
    {
        tracker[index] = i;
        DBG("*****HOLE FOUND!!!******* filling hole, index = %d, i - n = %d - %d\n\n", index, i, zl_configBlockSize - 1);
        for (k = i; k < zl_configBlockSize; k++)
            pCr2Buf[index].value[k] = (HBI_NO_OP_CMD << 8) | HBI_NO_OP_CMD;
        index++;
    }

    for (j = 0; j < index; j++)
    {
        unsigned char page = pCr2Buf[j].reg >> 8;
        unsigned char offset = (pCr2Buf[j].reg & 0xFF) >> 1;
        outbuf[byteCount++] = HBI_SELECT_PAGE_CMD;
        outbuf[byteCount++] = (page - 1);
        outbuf[byteCount++] = (offset);
        /*calculate HBI command length accordingly*/
        if (tracker[j])
            outbuf[byteCount++] = HBI_PAGE_WR_CMD_LOW_BYTE(tracker[j]);
        else
            outbuf[byteCount++] = HBI_PAGE_WR_CMD_LOW_BYTE(zl_configBlockSize);

        for (i = 0; i < zl_configBlockSize; i++)
        {
            outbuf[byteCount++] = (pCr2Buf[j].value[i] >> 8);
            outbuf[byteCount++] = (pCr2Buf[j].value[i] & 0xFF);
        }
        dumpFile(outbuf, byteCount);
        byteCount = 0;
    }

    free(pCr2Buf);
    numElements = index;
    total_len = index;;
    //DBG ("size of pCr2Buf = %u bytes.. \n", sizeof(pCr2Buf));
    /* write HBI header to file */
    if (bOutputTypeC)
    {
        outLog("};\n");
        outLog("#endif\n");
    }
    index = 0;
    zl_configBlockSize += 2;
    total_len *= (2 * zl_configBlockSize);

    outbuf[index++] = IMG_HDR_VERSION;
    outbuf[index++] = IMG_HDR_FORMAT(1);
    outbuf[index++] = (fw_opn_code >> 8) & 0xFF;
    outbuf[index++] = fw_opn_code & 0xFF;
    outbuf[index++] = (zl_configBlockSize) >> 8;
    outbuf[index++] = (zl_configBlockSize)& 0xFF;
    outbuf[index++] = total_len >> 24;
    outbuf[index++] = total_len >> 16;
    outbuf[index++] = total_len >> 8;
    outbuf[index++] = total_len & 0xFF;
    outbuf[index++] = 0; //reserved
    outbuf[index++] = 0; //reserved 
    fseek(saveFhande, current_fpos, SEEK_SET);
    dumpFile(outbuf, index);

    return;
}

void dumpFile(unsigned char *buf, int len)
{
    int j;

    if (bOutputTypeC)
    {
#if DBG_COUT
        for (j = 0; j < len; j += 2)
            outLog("0x%04X, ", *((unsigned short *)&(buf[j])));
#else
        for (j = 0; j < len; j++)
            outLog("0x%02X, ", buf[j]);
#endif
        outLog("\n");
    }
    else
    {
        fwrite(buf, 1, len, saveFhande);
#ifdef DISPLAY_TO_TERMINAL
        for (j = 0; j < len; j++)
            printf("0x%x ", buf[j]);
#endif
    }
    total_len += len;
    return;
}

/* HbiPagedBootImage() - This function reads and process
 * the Voice processing s3 file into a HBI PAGED write command
 * based image
 * FILE -- pointer to the location where to find the file
 */
void HbiPagedBootImage()
{
    char          line[1024] = "";
    int           rec_type;
    char          addrStr[5] = "";
    int           addrLen = 0;
    unsigned int  nextAddrToRead = 0xFFFFFFFF;
    unsigned int  byteCount = 0;
    int           addrLenNumByteMap[10] = { 4, 4, 6, 8, 0, 4, 0, 8, 6, 4 };
    int           hbi_cmd_indx = -1;
    unsigned int   outDataLen = 0;
    unsigned int  address;
    unsigned int  BaseAddr = 0xFFFFFFFF;
    unsigned char offset;
    int           j = 0, i;
    int           bCont = 0;
    int           total_len_indx;
    time_t        rawtime;
    struct tm     *timeinfo;

    /* required for C-output, a single byte entry is encoded as 0x<val>,
       totals upto 6
       */
    int chars_per_entry = 6;
    int offset_to_shift = 0;
    int current_fpos = 0;

    /*start at the beginning of the file*/
    fseek(BOOT_FD, 0, SEEK_SET);

    if (bOutputTypeC)
    {
        char *array_name = stringToUpperCase(strtok(outpath, "."));
        time(&rawtime);
        timeinfo = localtime(&rawtime);


        outLog("/*Source file %s, modified: %s */ \n", inpath, asctime(timeinfo));
        outLog("#ifndef __%s__\n", array_name);
        outLog("#define __%s__\n\n", array_name);

        outLog("const unsigned char %s[] ={\n", stringToLowerCase(array_name));

        /* save current file position to be used later while writing
           header
           */
        current_fpos = ftell(saveFhande);
        /* Add 1 for newline char which is added in C- output */
        offset_to_shift = (IMG_HDR_LEN * chars_per_entry) + 1;
    }
    else
        offset_to_shift = IMG_HDR_LEN;

    /*
       Leave space for header to be filled at the end of
       function.
       Push the file pointer by the required offset
       */
    fseek(saveFhande, offset_to_shift, SEEK_CUR);

    memset(outbuf, 0, sizeof(outbuf));


    /* HBI command to write page offset */
    outbuf[byteCount++] = HBI_SELECT_PAGE_CMD;
    outbuf[byteCount++] = 0xFF;

    while (fgets(line, 1024, BOOT_FD) != NULL)
    {
        int inPtr = 0;
        int inDataLen = 0;

        /* if this line is not an srecord skip it */
        if (line[inPtr++] != 'S')
        {
            continue;
        }

        /* get the srecord type */
        rec_type = line[inPtr++] - '0';

        /* skip non-existent srecord types and block header */
        if ((rec_type == 4) || (rec_type == 6) || (rec_type == 0))
        {
            continue;
        }

        /* get number of bytes */
        sscanf(&line[inPtr], "%02x", &inDataLen);

        inPtr += 2;

        /* get the info based on srecord type (skip checksum) */
        addrLen = addrLenNumByteMap[rec_type];

        sprintf(addrStr, "%%%ix", addrLen);

        sscanf(&line[inPtr], addrStr, &address);

        inDataLen -= (addrLen / 2);
        inDataLen -= FWR_CHKSUM_LEN;

        inPtr += addrLen;

        DBG("address 0x%x, InDataLen %d nextAddrToRead 0x%x\n",
            address, inDataLen, nextAddrToRead);

        addrLen = addrLen >> 1;

        if ((rec_type == 7) || (rec_type == 8) || (rec_type == 9))
        {
            if (bCont)
            {
                outbuf[hbi_cmd_indx] = ((outDataLen >> 1) - 1);
                bCont = 0;
            }
            else
            {
                outbuf[hbi_cmd_indx] = (0x80 | ((outDataLen >> 1) - 1));
            }
            outDataLen = 0;
            if ((byteCount >= zl_firmwareBlockSize) ||
                (byteCount > (zl_firmwareBlockSize - (HBI_SELECT_PAGE_CMD_LEN +
                HBI_PAGE_OFFSET_CMD_LEN +
                addrLen))))
            {
                dumpFile(outbuf, byteCount);
                byteCount = 0;
                outDataLen = 0;
            }
            /* write the address into Firmware Execution Address reg */
            outbuf[byteCount++] = HBI_SELECT_PAGE_CMD;
            outbuf[byteCount++] = (((HOST_FWR_EXEC_REG >> 8) & 0xFF) - 1);


            outbuf[byteCount++] = (HOST_FWR_EXEC_REG & 0xFF) >> 1;
            outbuf[byteCount++] = ((addrLen >> 1) - 1) | 0x80;

            for (i = (addrLen - 1); i >= 0; i--)
                outbuf[byteCount++] = ((address >> (8 * i)) & 0xFF);

            /* fill with HBI_NOOP CMD */
            while (byteCount < zl_firmwareBlockSize)
            {
                outbuf[byteCount++] = HBI_NO_OP_CMD;
                outbuf[byteCount++] = HBI_NO_OP_CMD;
            }

            /* write to file */
            dumpFile(outbuf, byteCount);

            printf("Firmware Read Complete\n");
            break;
        }

        if (address != nextAddrToRead)
        {

            DBG("Discontinuous Address !!!!!\n");
            if (hbi_cmd_indx >= 0)
            {
                if (bCont)
                {
                    outbuf[hbi_cmd_indx] = ((outDataLen >> 1) - 1);
                    bCont = 0;
                }
                else
                {
                    outbuf[hbi_cmd_indx] = (0x80 | ((outDataLen >> 1) - 1));
                }
                outDataLen = 0;
            }

            DBG(" %d : byteCount %d, nextAddrToRead 0x%x\n",
                __LINE__, byteCount, nextAddrToRead);

            /* every HBI chunk need one full complete command
               thus check if length of remaining buffer is enough to
               hold one complete HBI command*/
            if (byteCount >= (zl_firmwareBlockSize -
                (HBI_DIRECT_PAGE_ACCESS_CMD_LEN + addrLen +
                HBI_PAGE_OFFSET_CMD_LEN +
                HBI_PAGED_OFFSET_MIN_DATA_LEN)))
            {
                /* dump data into output file */
                DBG("%d Insufficient space\n", __LINE__);

                while (byteCount < zl_firmwareBlockSize)
                {
                    outbuf[byteCount++] = 0xFF;
                    outbuf[byteCount++] = 0xFF;
                }

                dumpFile(outbuf, byteCount);

                byteCount = 0;
                outDataLen = 0;
            }

            /* 1. write page 255 base address register 0x000C */
            outbuf[byteCount++] = HBI_DIRECT_PAGE_ACCESS_CMD |
                ((PAGE255_REG & 0xFF) >> 1);
            outbuf[byteCount++] = ((addrLen >> 1) - 1) | 0x80;

            BaseAddr = address & 0xFFFFFF00;

            offset = (address & 0xFF) >> 1;

            /* write data MSB */
            outbuf[byteCount++] = (BaseAddr >> 24) & 0xFF;
            outbuf[byteCount++] = (BaseAddr >> 16) & 0xFF;
            outbuf[byteCount++] = (BaseAddr >> 8) & 0xFF;
            outbuf[byteCount++] = BaseAddr & 0xff;

            outbuf[byteCount++] = offset;
            hbi_cmd_indx = byteCount++;  /* save for later use */

            nextAddrToRead = address;
        }

        /* copy block data into a buffer */
        for (i = 0; i < inDataLen; i++, inPtr += 2)
        {
            DBG("i %d byteCount %d, outDataLen %d " \
                "nextAddrToRead 0x%x\n", i, byteCount, outDataLen, nextAddrToRead);

            if (outDataLen > 255)
            {
                printf("Error ! HBI Command payload exceeded max allowed "\
                    "for single write \n");
                return;
            }

            if ((byteCount >= zl_firmwareBlockSize) ||
                (nextAddrToRead >= BaseAddr + HBI_MAX_PAGE_LEN))
            {
                DBG("dump data to file\n");

                /* data that have been written
                   into outbuffer */

                if (bCont)
                {
                    outbuf[hbi_cmd_indx] = ((outDataLen >> 1) - 1);
                    bCont = 0;
                }
                else
                {
                    outbuf[hbi_cmd_indx] = (0x80 | ((outDataLen >> 1) - 1));
                }
                outDataLen = 0;

                if (nextAddrToRead >= (BaseAddr + HBI_MAX_PAGE_LEN))
                {
                    DBG("Continuous Wr exceeded maximum supported \n");

                    BaseAddr = nextAddrToRead & 0xFFFFFF00;
                    offset = (nextAddrToRead & 0xFF) >> 1;

                    addrLen = sizeof(BaseAddr);

                    if (byteCount >= (zl_firmwareBlockSize -
                        (HBI_DIRECT_PAGE_ACCESS_CMD_LEN +
                        addrLen + 2)))
                    {

                        while (byteCount < zl_firmwareBlockSize)
                        {
                            DBG("Line %d fill with NOOP\n", __LINE__);
                            outbuf[byteCount++] = 0xFF;
                            outbuf[byteCount++] = 0xFF;
                        }

                        dumpFile(outbuf, byteCount);

                        byteCount = 0;

                    }


                    /* 1. write page 255 base address register 0x000C */
                    outbuf[byteCount++] = HBI_DIRECT_PAGE_ACCESS_CMD |
                        ((PAGE255_REG & 0xFF) >> 1);
                    outbuf[byteCount++] = ((addrLen >> 1) - 1) | 0x80;

                    /* write data MSB */
                    outbuf[byteCount++] = (BaseAddr >> 24) & 0xFF;
                    outbuf[byteCount++] = (BaseAddr >> 16) & 0xFF;
                    outbuf[byteCount++] = (BaseAddr >> 8) & 0xFF;
                    outbuf[byteCount++] = BaseAddr & 0xFF;

                    outbuf[byteCount++] = offset;
                }
                else
                {
                    /* dump data into output file */
                    dumpFile(outbuf, byteCount);

                    byteCount = 0;
                    if (i < inDataLen - 1)
                    {
                        /* continue writing remaining data into buffer
                           using continue paged access command*/
                        DBG("continue page wr\n");
                        outbuf[byteCount++] = HBI_CONT_PAGED_WR_CMD;
                        bCont = 1;
                    }
                }
                hbi_cmd_indx = byteCount++;  /* save for later use */
            }

            sscanf(&line[inPtr], "%2x", (unsigned int *)&outbuf[byteCount++]);
            outDataLen++;
            nextAddrToRead++;
        }
    }  /*while end*/

    if (bOutputTypeC)
    {
        outLog("};\n");
        outLog("#endif\n");
    }

    /* write HBI header to file */
    i = 0;

    outbuf[i++] = IMG_HDR_VERSION;
    outbuf[i++] = IMG_HDR_FORMAT(0);
    outbuf[i++] = (fw_opn_code >> 8) & 0xFF;
    outbuf[i++] = fw_opn_code & 0xFF;
    outbuf[i++] = (zl_firmwareBlockSize >> 1) >> 8;
    outbuf[i++] = (zl_firmwareBlockSize >> 1) & 0xFF;
    outbuf[i++] = total_len >> 24;
    outbuf[i++] = total_len >> 16;
    outbuf[i++] = total_len >> 8;
    outbuf[i++] = total_len & 0xFF;
    outbuf[i++] = 0; //reserved
    outbuf[i++] = 0; //reserved

    if (i > IMG_HDR_LEN)
    {
        printf("Something wrong with header\n");
        return;
    }

    /* set to the header position */
    fseek(saveFhande, current_fpos, SEEK_SET);

    dumpFile(outbuf, i);

    DBG("total length of data written %d, block size %d\n", total_len, zl_firmwareBlockSize);
    return;
}


/* FUNCTION NAME :  outLog()
 * DESCRIPTION   :  This function is alternative to printf. It can output a buffer to a terminal or save it to disk
 */
void outLog(const char *str, ...)
{
    va_list ap;
    char buffer[600];
    va_start(ap, str);
    vsprintf(buffer, str, ap);
    va_end(ap);

    fputs(buffer, saveFhande); /*send to file*/

#ifdef DISPLAY_TO_TERMINAL
    printf(buffer); /*send to display terminal*/
#endif /*DISPLAY_TO_TERMINAL*/ 
}

int main(int argc, char** argv)
{
    long index = 0, i = 0;
    char *p1;
    int flag = 0;
    unsigned short block_Size = 16;
    int c;

    while ((c = getopt(argc, argv, "i:o:b:f:h")) != -1)
    {
        switch (c){

        case 'i':
            inpath = optarg;
            DBG("inpath %s\n", inpath);
            break;

        case 'o':
            outpath = optarg;
            DBG("outpath %s\n", outpath);
            break;

        case 'b':
            block_Size = (unsigned short)strtoul(optarg, NULL, 0);
            DBG("block size %u\n", block_Size);
            break;

        case'f':
            fw_opn_code = strtoul(optarg, NULL, 0);
            DBG("fw_opn_code %u\n", fw_opn_code);
            break;

        case 'h':

            printf("Usage: %s -i [input filename] -o [output filename.bin/.c] " \
                "-b [block size] -f [firmware code] \n", argv[0]);

            printf(" -i: input image file (.s3 or .cr2) \n "\
                " -b: block size in unit of words with 16-bit word length \n" \
                " -o : output file name (please use .c as file extension " \
                "for C output \n" \
                " -f : firmware code \n");

            printf("Image identification whether firmware or configuration " \
                "record is done dynamic based on file extension\n");

            printf("Example: to generate C file for zl38040 in blocks of 128 WORDS " \
                "with input file name ZLS38040_v1.0.10.s3 and output " \
                "zl38040_firmware.c " \
                "%s -i ZLS38040_v1.0.10.s3 -o zl38040_firmware.c -b 128 " \
                "-f 38040\n", argv[0]);

            printf("Block size: \n for *.s3 is 16*2^n where n =(0, 1, 2, 3)" \
                " \n for *.cr2 is 1*2^n where n =(0, 1, 2, 3, 4, 5, 6, 7)\n");

            return;
        }
    }

    if (inpath == NULL || outpath == NULL)
    {
        printf("Argument not been given appropraitly. please run %s -h\n", argv[0]);
        return;
    }

    /*check whether to convert a *.s3 or a*.cr2 file*/
    p1 = strstr(inpath, ".s3");

    if (p1 != NULL)
    {
        flag = 1;
        if (!fw_opn_code)
        {
            printf("Need firmware code as input. for usage please run %s -h\n",
                argv[0]);
        }
        if (check_fwrblock_size(block_Size) < 0)
        {
            printf("   WARNING!!! Invalid block size %d\n" \
                "   firmware block size must be a number that is a multiple of 16\n" \
                "   as per this equation 16*2^n where n =(0, 1, 2, 3)\n" \
                "   should be a value from 16 to 128\n", block_Size);
            printf("\nconverting the firmware in blocks of 16 words...\n");
            block_Size = 16;
        }
    }
    else
    {
        flag = 0;
        if (check_cfgblock_size(block_Size) < 0)
        {
            printf("   WARNING!!! Invalid Block size %d \n" \
                "   config block size must be a value from 1 to 128 \n" \
                "   as per this equation 1*2^n where n =(0, 1, 2, 3, 4, 5, 6, 7)\n", block_Size);
            printf("\nconverting the config in blocks of 1 words...\n");
            block_Size = 1;
        }
    }

    BOOT_FD = fopen(inpath, "rb");
    if (BOOT_FD == NULL)
    {
        printf("Couldn't open %s file\n", inpath);
        return -1;
    }

    if ((strstr(outpath, ".c") != NULL))
    {
        DBG("Output type is 'C' file\n");
        bOutputTypeC = 1;
    }

    /* open file */
    saveFhande = fopen(outpath, "wb");
    if (saveFhande == NULL)
    {
        printf("Cannot open debug file %s for writing.\n", outpath);
        return -1;
    }


    printf("%s convertion in progress...Please wait\n", inpath);


    len = fseekNunlines(BOOT_FD);
    if (len <= 0)
    {
        printf("Error: file is not of the correct format...\n");
        return -1;
    }
    DBG("total number of lines %d\n", len);

    rewind(BOOT_FD);
    if (flag)
    {

        zl_firmwareBlockSize = block_Size * 2;
        HbiPagedBootImage();
        fclose(saveFhande);
    }
    else
    {
        if (bOutputTypeC)
        {
            char *p;
            time_t rawtime;
            struct tm * timeinfo;
            time(&rawtime);
            timeinfo = localtime(&rawtime);

            p = strtok(outpath, ".");
            p = strcat(p, ".c");
            outLog("/*Source file %s, modified: %s */ \n", inpath, asctime(timeinfo));
        }
        zl_configBlockSize = block_Size;
        readCfgFile();
        fclose(saveFhande);

    }
    fclose(BOOT_FD);

    printf("%s conversion completed successfully...\n", inpath);

    return 0;
}

