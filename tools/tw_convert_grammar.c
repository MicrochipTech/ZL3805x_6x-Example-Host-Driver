/********************************************/
/*! \mainpage
*
* \section intro_sec Introduction
*
* This is an document summarizes Microsemi VPROC Device Grammar Convertor tool.
*
* \section BI Build Instructions
*
* Use any compiler on host machine.
*
* Example Make Command:
*
* gcc tw_convert_grammar.c -o tw_convert_grammar
*
* Usage:
*
* tw_convert_grammar <options>
*
* Options:
*
*
*        -t <trigger acoustic model (*.bin) >
*        -c <command acoustic model (*.bin) >
*        -o <output file name (*.bin) >
*        -d <grammar description >(max 32 bytes length)
*
* To Display help menu
*
* tw_convert_grammar -h
*
********************************************************************************/
#include <stdint.h>
#include <stdlib.h> /* malloc, free, rand */
#include <stdio.h>  /*getline(), etc...*/
#include <stdarg.h>
#include <string.h> 
#include <unistd.h>
#include <time.h>       /* time_t, struct tm, time, localtime */
#include <byteswap.h>

#define _byteswap_ulong(x)	bswap_32(x) 

#undef DEBUG
#ifdef DEBUG
#define DBG printf
#define DEBUG_PRINTF printf
#else
#define DBG
#define DEBUG_PRINTF
#endif

typedef struct AT_RawFileType {
    FILE *fd;           /* file descriptor to file */
    uint32_t sizeOfData;   /* number of linear samples in data */
} AT_RawFileType;

/* 64 byte header */
typedef struct {
    unsigned int trigAcousticModelOffset;
    unsigned int trigAcousticModelSize;
    unsigned int cmdAcousticModelOffset;
    unsigned int cmdAcousticModelSize;
    char description[32];
    unsigned int version;
    short numTriggers;
    short numCommands;
    unsigned int trigParamOffset;
    unsigned int cmdParamOffset;
} Grammar_Header_Retune_V1;

#define BLOB_BASE_OFFSET		(2*sizeof(Grammar_Header_Retune_V1))
// Return status codes functions.
typedef enum {
    AT_STATUS_SUCCESS = 0x00,
    AT_STATUS_FAILURE = 0x01,
    AT_STATUS_INVALID_ARG = 0x02,
} AT_Status;

char *trig_acousticmdl, *cmd_acousticmdl, *desc, *opgrammarFile;
int bOutputTypeC = 0;
AT_RawFileType outFile;
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

    fputs(buffer, outFile.fd); /*send to file*/

#ifdef DISPLAY_TO_TERMINAL
    printf(buffer); /*send to display terminal*/
#endif /*DISPLAY_TO_TERMINAL*/ 
}
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
// Function to implement strncpy() function
char* my_strncpy(char* str2, const char* str1, size_t num)
{
    char* ptr = str2;

    if (str2 == NULL) {
        return NULL;
    }
    while (*str1 && num--)
    {
        *str2 = *str1;
        str2++;
        str1++;
    }
    // null terminate destination string
    *str2 = '\0';

    return ptr;
}
AT_Status CreateGrammarBin(
    const char *trigAcousticModel,
    const char *cmdAcousticModel,
    const char *description, /* will truncate to max 32 bytes */
    const unsigned int version,
    const char *outputFile)
{
    AT_Status status = AT_STATUS_INVALID_ARG;

    unsigned int i = 0, j;
    unsigned int padding1 = 0;
    unsigned int padding2 = 0;
    unsigned int padding3 = 0;
    int filenamelenth;
    char binByte = 0;
    Grammar_Header_Retune_V1 grammarHdr;
    AT_RawFileType trigM;
    AT_RawFileType cmdM;
    AT_RawFileType trigP;
    AT_RawFileType cmdP;
    char tempFileName[1024];
    unsigned int grammar_size;

    memset(&grammarHdr, 0, sizeof(Grammar_Header_Retune_V1));
    memset(&trigM, 0, sizeof(AT_RawFileType));
    memset(&cmdM, 0, sizeof(AT_RawFileType));
    memset(&trigP, 0, sizeof(AT_RawFileType));
    memset(&cmdP, 0, sizeof(AT_RawFileType));
    memset(&outFile, 0, sizeof(AT_RawFileType));

    if ((trigAcousticModel != NULL) && (strlen(trigAcousticModel) != 0) && (trigM.fd = fopen(trigAcousticModel, "rb")) == NULL) {
        DEBUG_PRINTF("filename %ls is not accessible", trigAcousticModel);
        status = AT_STATUS_INVALID_ARG;
        goto end;
    }

    if ((cmdAcousticModel != NULL) && (strlen(cmdAcousticModel) != 0) && (cmdM.fd = fopen(cmdAcousticModel, "rb")) == NULL) {
        DEBUG_PRINTF("filename %ls is not accessible", cmdAcousticModel);
        status = AT_STATUS_INVALID_ARG;
        goto end;
    }

    if ((outputFile == NULL) || (outFile.fd = fopen(outputFile, "wb")) == NULL) {
        DEBUG_PRINTF("filename %ls is not accessible", outputFile);
        status = AT_STATUS_INVALID_ARG;
        goto end;
    }
    if (bOutputTypeC)
    {
        char *p;
        time_t rawtime;
        struct tm * timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        p = strtok(outputFile, ".");
        p = strcat(p, ".c");
        outLog("/*trigger model file %s, modified: %s */ \n", trigAcousticModel, asctime(timeinfo));
        outLog("/*command model file %s, modified: %s */ \n", cmdAcousticModel, asctime(timeinfo));
    }

    trigM.sizeOfData = 0;
    trigP.sizeOfData = 0;
    if (trigM.fd != 0) {
        fseek(trigM.fd, 0, SEEK_END);
        trigM.sizeOfData = ftell(trigM.fd);
        fseek(trigM.fd, 0, SEEK_SET);

        filenamelenth = strlen(trigAcousticModel) - 4;
        if ((filenamelenth + 12) > 1024) {
            filenamelenth = 1024 - 12;
        }
        my_strncpy(tempFileName, trigAcousticModel, filenamelenth);
        strncat(tempFileName, "_params.bin", 11);
        if ((trigP.fd = fopen(tempFileName, "rb")) != NULL) {
            fseek(trigP.fd, 0, SEEK_END);
            trigP.sizeOfData = ftell(trigP.fd);
            fseek(trigP.fd, 0, SEEK_SET);
        }
    }

    cmdM.sizeOfData = 0;
    cmdP.sizeOfData = 0;
    if (cmdM.fd != 0) {
        fseek(cmdM.fd, 0, SEEK_END);
        cmdM.sizeOfData = ftell(cmdM.fd);
        fseek(cmdM.fd, 0, SEEK_SET);

        filenamelenth = strlen(cmdAcousticModel) - 4;
        if ((filenamelenth + 12) > 1024) {
            filenamelenth = 1024 - 12;
        }
        my_strncpy(tempFileName, cmdAcousticModel, filenamelenth);
        strncat(tempFileName, "_params.bin", 11);
        if ((cmdP.fd = fopen(tempFileName, "rb")) != NULL) {
            fseek(cmdP.fd, 0, SEEK_END);
            cmdP.sizeOfData = ftell(cmdP.fd);
            fseek(cmdP.fd, 0, SEEK_SET);
        }
    }

    if (!trigM.sizeOfData && !cmdM.sizeOfData) {
        DEBUG_PRINTF("No Retune Grammar specified");
    }

    if (trigM.sizeOfData) {
        grammarHdr.trigAcousticModelOffset = BLOB_BASE_OFFSET;
        grammarHdr.trigAcousticModelSize = trigM.sizeOfData;
    }
    else {
        grammarHdr.trigAcousticModelOffset = 0x0;
        grammarHdr.trigAcousticModelSize = 0x0;
    }

    // Determine if we will need to add padding to get the subsequent parameter blob to be 4-byte aligned.
    padding1 = (trigM.sizeOfData) & 0x0003;
    if (padding1 != 0)
        padding1 = 4 - padding1;

    // Determine if we will need to add padding to get the subsequent command grammar blob to be 16-byte aligned.
    padding2 = ((trigM.sizeOfData) + (padding1)+(trigP.sizeOfData)) & 0x000F;

    if (padding2 != 0)
        padding2 = 16 - padding2;

    // Determine if we will need to add padding to get the subsequent parameter blob to be 4-byte aligned.
    padding3 = (cmdM.sizeOfData) & 0x0003;
    if (padding3 != 0)
        padding3 = 4 - padding3;

    if (cmdM.sizeOfData) {
        // Set the 16-byte aligned offset to the command grammar.
        grammarHdr.cmdAcousticModelOffset = BLOB_BASE_OFFSET + trigM.sizeOfData + padding1 + trigP.sizeOfData + padding2;
        grammarHdr.cmdAcousticModelSize = cmdM.sizeOfData;
    }
    else {
        grammarHdr.cmdAcousticModelOffset = 0;
        grammarHdr.cmdAcousticModelSize = 0;
    }

    if (trigP.sizeOfData != 0) {
        grammarHdr.trigParamOffset = (BLOB_BASE_OFFSET + trigM.sizeOfData + padding1);
    }
    if (cmdP.sizeOfData != 0) {
        grammarHdr.cmdParamOffset = (BLOB_BASE_OFFSET + trigM.sizeOfData + padding1 + trigP.sizeOfData + padding2 + cmdM.sizeOfData + padding3);
    }
    grammar_size = grammarHdr.cmdParamOffset + cmdP.sizeOfData;
    grammarHdr.trigAcousticModelOffset = _byteswap_ulong(grammarHdr.trigAcousticModelOffset);
    grammarHdr.trigAcousticModelSize = _byteswap_ulong(grammarHdr.trigAcousticModelSize);
    grammarHdr.cmdAcousticModelOffset = _byteswap_ulong(grammarHdr.cmdAcousticModelOffset);
    grammarHdr.cmdAcousticModelSize = _byteswap_ulong(grammarHdr.cmdAcousticModelSize);

    grammarHdr.trigParamOffset = _byteswap_ulong(grammarHdr.trigParamOffset);
    grammarHdr.cmdParamOffset = _byteswap_ulong(grammarHdr.cmdParamOffset);

    memcpy(grammarHdr.description, description, strlen(description));
    grammarHdr.version = _byteswap_ulong(version);
    if (bOutputTypeC)
    {
        char *array_name = stringToUpperCase(strtok(outputFile, "."));
        outLog("#ifndef __%s__\n", array_name);
        outLog("#define __%s__\n\n", array_name);

        outLog("const unsigned char %s[] ={\n", stringToLowerCase(array_name));
    }
    /* Grammar binary for TW consists of 2 copies of the header followed by AM, size, CM and size */
    if (bOutputTypeC)
    {
        unsigned char *HdrPtr = (unsigned char *)&grammarHdr;
        for (j = 0; j < sizeof(Grammar_Header_Retune_V1); j++)
        {
            outLog("0x%02X, ", HdrPtr[j]);
        }

        for (j = 0; j < sizeof(Grammar_Header_Retune_V1); j++)
        {
            outLog("0x%02X, ", HdrPtr[j]);
        }

    }
    else
    {
        fwrite(&grammarHdr, sizeof(Grammar_Header_Retune_V1), 1, outFile.fd);
        fwrite(&grammarHdr, sizeof(Grammar_Header_Retune_V1), 1, outFile.fd);
    }
    i = 0;
    while (i++ < trigM.sizeOfData) {
        fread(&binByte, sizeof(char), 1, trigM.fd);
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    binByte = 0;
    for (i = 0; i < padding1; i++) {
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    i = 0;
    while (i++ < trigP.sizeOfData) {
        fread(&binByte, sizeof(char), 1, trigP.fd);
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    // Add any padding needed for getting the command grammar 16-byte aligned.
    binByte = 0;
    for (i = 0; i < padding2; i++) {
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    i = 0;
    while (i++ < cmdM.sizeOfData) {
        fread(&binByte, sizeof(char), 1, cmdM.fd);
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    binByte = 0;
    for (i = 0; i < padding3; i++) {
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    i = 0;
    while (i++ < cmdP.sizeOfData) {
        fread(&binByte, sizeof(char), 1, cmdP.fd);
        if (bOutputTypeC)
        {
            outLog("0x%02X, ", binByte);
        }
        else
        {
            fwrite(&binByte, sizeof(char), 1, outFile.fd);
        }
    }

    status = AT_STATUS_SUCCESS;

end:
    if (bOutputTypeC)
    {
        outLog("};\n");
        outLog("const unsigned int grammar_size = %d; \n", grammar_size);
        outLog("#endif\n");
    }
    if (trigM.fd) {
        fclose(trigM.fd);
    }

    if (trigP.fd) {
        fclose(trigP.fd);
    }

    if (cmdM.fd) {
        fclose(cmdM.fd);
    }

    if (cmdP.fd) {
        fclose(cmdP.fd);
    }

    if (outFile.fd) {
        fclose(outFile.fd);
    }


    return status;
}


/* ------------------------------------------------------------ */
int main(int argc, char** argv) {
    int c;

    while ((c = getopt(argc, argv, "t:c:o:d:h")) != -1)
    {
        switch (c){

        case 't':
            trig_acousticmdl = optarg;
            DBG("trigAcousticModel %s\n", trig_acousticmdl);
            break;

        case 'o':
            opgrammarFile = optarg;
            DBG("outputFile %s\n", opgrammarFile);
            break;

        case 'c':
            cmd_acousticmdl = optarg;
            DBG("cmdAcousticModel %s\n", cmd_acousticmdl);
            break;

        case 'd':
            desc = optarg;
            DBG("Grammar Description %s\n", desc);
            break;

        case 'h':

            printf("Usage: %s -t [trigAcousticModel.bin] -c [cmdAcousticModel.bin] " \
                "-d [grammar description] -o [outputFile.bin] \n", argv[0]);

            printf(" -t: trigger acoustic model (*.bin) \n "\
                " -c: command acoustic model (*.bin) \n "\
                " -o: output file name (*.bin) \n " \
                " -d: grammar description (max 32 bytes length)\n");
            return -1;
        }
    }
    if (trig_acousticmdl == NULL || opgrammarFile == NULL)
    {
        printf("Argument not been given appropraitly. please run %s -h\n", argv[0]);
        return -1;
    }
    if ((strstr(opgrammarFile, ".c") != NULL))
    {
        DBG("Output type is 'C' file\n");
        bOutputTypeC = 1;
    }
    CreateGrammarBin(trig_acousticmdl, cmd_acousticmdl, desc, 1, opgrammarFile);
    return 0;
}
