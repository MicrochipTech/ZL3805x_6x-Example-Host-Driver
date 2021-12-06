# Getting Started Guide: ZLS3805x_6x access over SPI/I2C Example Code

#### Devices: | ZLS38052 | ZLS38062 | ZLS38063 | ZLS38064 | ZLS38066 |

## Introduction

This repository has code for the following examples:

    1) Firmware Converter Tool.

    2) Load ZLS3805x_6x firmware/Configuration images over SPI/I2C on a Linux platform.

    3) SPI/I2C Read/Write, Erase the whole flash example code.

 The example code has been tested on raspberry pi platform based on Linux kernel 5.4.83-v7+.

You may find the following reference documents useful while using this guide:

• [SPI/I2C enable on raspberry pi platform](https://www.raspberrypi.com/documentation/computers/configuration.html).

• [Linux spidev_test.c file](https://github.com/torvalds/linux/blob/master/tools/spi/spidev_test.c).

• ZLS3805X_6X firmware manual.

### **1. Firmware Converter Tool**

This package uses a firmware/config converter tool which converts *.s3 and *.cr2 files to *.bin/*.c format.

To compile the C version of the converter tool, issue the below commands in a terminal window:

```c

cd /home/pi/ZL3805x_6x-Example-Host-Driver/tools/

gcc twConvertFirmware2c.c -o twConvertFirmware2c

sudo cp twConvertFirmware2c /usr/local/bin
```

Copy the *.s3 and *.cr2 to a desired location within the Pi. See the example command below on how to use the tool to convert a *.s3 and *.cr2 file.

Example: Let’s say I have a firmware *.s3 file named _Microsemi_ZLS38063.1_E0_10_0_firmware.s3_ and a configuration *.cr2 file generated from the Microsemi MiTuner tool named _Microsemi_ZLS38063.1_E0_10_0_config.cr2_ that are located in a directory named /home/pi/ZL3805x_6x-Example-Host-Driver/tools/ on the Pi. To convert the files to *.bin/*.c issue the following command sequence from a terminal on the Pi.

```c

twConvertFirmware2c -i Microsemi_ZLS38063.1_E0_10_0_firmware.s3 -o fwr.c -b 16 -f 38063 

twConvertFirmware2c -i Microsemi_ZLS38063.1_E0_10_0_config.cr2 -o config.c -b 16 -f 38063
```

The following two C files will be created within the folder

fwr.c

config.c

These files have a table with firmware/config data values, which has the same name as the file names.

Copy the *.c files to ZL3805x_6x-Example-Host-Driver/load_firmware_example folder.

```c
cp fwr.c ../load_firmware_example/
cp config.c ../load_firmware_example/
```

### **2. Load Firmware/Config Example**

To load firmware/config images into the device, build hbi_load_firmware target using the makefile from ZL3805x_6x-Example-Host-Driver folder.

```c
make hbi_load_firmware
sudo cp hbi_load_firmware /usr/local/bin
```

load_firmware_example.c file is accessing the firmware and config tables from fwr.c and config.c file respectively. These generated *.c files have the following tables with the name same as that of file names.

const unsigned char fwr[];

const unsigned char config[];

The user needs to use the exact same name for the converted files(fwr.c and config.c) to compile hbi_load_firmware without any errors. Otherwise change the table name in load_firmware_example.c accordingly.

To load firmware and config files and to save it to flash issue the following command.

```c
hbi_load_firmware
```

We can skip the 'save to flash' functionality by modifying _bSaveToFlash_ variable value in load_firmware_example.c file.

### **3. Read/Write Example**

To Read/Write specific registers of the ZL380xx device use the read_write_example example code commands as below.

```c
make rd_wr_test
sudo cp rd_wr_test /usr/local/bin
rd_wr_test
```

This code shows write and read example on register 0x000E.Also it has an example code for erasing the entire flash. This example shows how to issue a host command to the device.

Note: _Register 0x000E of the ZL380xx is a special register, whatever a host writes to this register will be zeroed out by the ZL380xx firmware, in order to confirm to the host that the command has been received correctly. Therefore, this register is a good way to verify that the host is accessing the device and the device is working properly._

### **4. Grammar Converter Tool**

This package uses a grammar converter tool which converts trigger and command models to a single *.bin/*.c format.

To compile the C version of the converter tool, issue the below commands in a terminal window:

```c

cd /home/pi/ZL3805x_6x-Example-Host-Driver/tools/

gcc tw_convert_grammar.c -o tw_convert_grammar

sudo cp tw_convert_grammar /usr/local/bin
```

Copy command and trigger models to a desired location within the Pi. See the example command below on how to use the tool to convert it to *.bin/*.c format. If you have separate params files for trigger and command models, make sure that it has exact similar name with <_command/trigger model name_>_params name convention.

Example: Let’s say I have a trigger acoustic model named _HeyVoiceSpot-27-rc_ and a configuration command acoustic model named _acoustic-command-rc2_ that are located in a directory named /home/pi/ZL3805x_6x-Example-Host-Driver/tools/ on the Pi. Params file should have the naming convention as_HeyVoiceSpot-27-rc_params_ and _acoustic-command-rc2_params_ and should place them in the same folder as trigger and command model located. To convert the grammar file to *.bin/*.c format, issue the following command sequence from a terminal on the Pi.

```c

tw_convert_grammar -t HeyVoiceSpot-27-rc -c acoustic-command-rc2 -o grammar.c -d retune 

```

The following C file will be created within the folder

grammar.c

This file has a table with grammar data values, which has the same name as the file names.

Copy the *.c file to ZL3805x_6x-Example-Host-Driver/load_grammar_example folder.

```c
cp grammar.c ../load_grammar_example/

```

### **5. Load Grammar Example**

To load firmware/config images into the device, build hbi_load_firmware target using the makefile from ZL3805x_6x-Example-Host-Driver folder.

```c
make hbi_load_grammar
sudo cp hbi_load_grammar /usr/local/bin
```

load_grammar_example.c file is accessing grammar table and size information from grammar.c file . This generated *.c file has the following parameters with the name same as that of file names.

const unsigned char grammar[];

const unsigned int grammar_size;

The user needs to use the exact same name for the converted file(grammar.c) to compile hbi_load_grammar without any errors. Otherwise change the table name in load_grammar_example.c accordingly.

To load grammar, issue the following command.

```c
hbi_load_grammar
```

## I2C Interface

We can configure SPI/I2C code from hbi.c file. Enable I2C macro for using this code in I2C environment. Also please note the following pin connection details for setting up TimberWolf device in I2C mode. For more detailed information please refer ZL380XX Datasheet and Firmware manual. 

![image](https://user-images.githubusercontent.com/89809326/144310390-a781d8c5-8bdc-4655-93bc-eb52d4b92db8.png)

![image](https://user-images.githubusercontent.com/89809326/144310542-438e742e-2a11-4259-ab64-85863824dd78.png)