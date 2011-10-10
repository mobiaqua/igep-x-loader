#
# (C) Copyright 2009-2011 ISEE
# Manel Caro (mcaro@iseebcn.com)
#
# Version: IGEP-X-Loader 2.2.0-1
# 
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

Index
=====

1 Summary.
2 Features and Issues.
2.1 Improvements & Modifications.
2.2 Issues
2.3 TODO
2.4 Version Changes
4 Settings & Configuration
4.1 MMC Boot
4.2 Setup igep.ini file
4.3 Boot Priority
4.4 OneNand Partition settings
4.4.1 Xloader partition
4.4.2 Boot Partition
4.4.3 Rootfs
5 Build procedure
5.1 Build with Ubuntu Cross Compiler gcc 4.5.1
5.2 Build with IGEP SDK
5.3 Build Native
6 Contribution & Support & Report Bugs

1 Summary:
===========
This directory contains the source code for X-Loader, an initial program
loader for Embedded boards based on OMAP processors.

2 Features and Issues:
=======================

2.1 Improvements & Modifications
---------------------------------

* Added malloc/free functionality.
* Added mtd framework and onenand support, removed the old onenand drivers.
* Added fs jffs2 support using mtd & onenand support (Read Only).
* Added crc32 and zlib. 
* Jffs2 zlib compression support (Read Only).
* Dual boot mmc & onenand with mmc highest priority.
* Added Linux kernel boot directly (Support for 2.6.22 and highest version kernels)
* Linux kernel supported images: vmlinuz, bzImage and zImage.
* Support for loading Linux Ram disk (EXPERIMENTAL)
* Added "ini" files parser.
* The configuration resides in a plain txt (ini format file).
* Support Windows & Linux formating ini files.
* boot from mmc, onenand, or mix with mmc highest priority.
* Added codeblocks project and compilation rules.
* Added support for vfat32 extra names.
* Configure TPS65950 voltage to 1.35V if it's used a DM3730 processor.
* Added new parameter MachineID in kernel tag file, with it you can
	- configure the kernel board ID setup
* Added new parameter buddy for kernels 2.6.35.13-3
* Optimize some LPDDR Memory configuration values
* Removed some OneNand Debug information
* Removed some FAT incorrect warnings
* Only support gcc 4.5.2 linaro version
* Added Support Initial Ram disk
* Reconfigure Makefile options
* Support Kernels 2.6.35 and 2.6.37

2.2 Issues
-----------

* The ini file it's limited to max size: 16 KiB
  This is not a real limitation due all ini file it's 
  copied into the RAM memory.
* Kernel Command line parameters it's limited to: 4 KiB
  This is not a real limitation due all ini file it's 
  copied into the RAM memory.
* Malloc it's limited to 32 MiB (this is not a real issue)

2.3 TODO
--------

* Remove compilation warnings.
* Add in the tag=value inline comments

2.4 Version Changes
-------------------
[2.1.0-1] This version only can be build with gcc linaro 4.5.2 other compilers be not supported.
[2.1.0-1] Removed some uncontrolled "printf" with incorrect information.
[2.1.0-1] Modified some code under __DEBUG__ option.
[2.1.0-1] Added Support for TPS65950-A3 initialization at 1.35V
[2.1.0-1] Added support for IGEP Module 0030
[2.1.0-1] Added support dynamic Machine ID selection (same xloader boot IGEPv2 & IGEP Module)
----
[2.1.0-2] ARM Compilation bug resolved
----
[2.1.0-3] Update SDRAM structure initialization
[2.1.0-3] Added support for Initial RAM disk
[2.1.0-3] Updated Initial RAM disk destination address
----
[2.2.0-1] Update Makefile structure
[2.2.0-1] Downgrade the boot processor speed

3 Status:
==========

* Support IGEP0020 Revision B & C family boards.
	- Tested with IGEPv2 (DM3730@1Ghz and 512/512 MB Ram/Onenand)
        - Tested with IGEPv2 (AM3703@1Ghz and 512/512 MB Ram/Onenand)
	- Tested with IGEPv2 (OMAP3530@720Mhz and 512/512 MB Ram/Onenand)
* Support IGEP0030 Revisions D & E family Modules.
	- Tested with IGEP Module (DM3730@1Ghz and 512/512 MB Ram/Onenand) + BaseBoard Revision A
	- Tested with IGEP Module (AM3703@1Ghz and 512/512 MB Ram/Onenand) + BaseBoard Revision A
	- Tested with IGEP Module (DM3730@1Ghz and 128/256 MB Ram/Onenand) + BaseBoard Revision A/B
	- Tested with IGEP Module (AM3703@1Ghz and 128/256 MB Ram/Onenand) + BaseBoard Revision A/B
	

4 Settings & Configuration:
============================

4.1 MMC Boot
------------
Get a new mmc and create two partitions, the first one must be fat (you can follow 
this howto: http://code.google.com/p/beagleboard/wiki/LinuxBootDiskFormat)
In this first partition (boot partition) you should copy:

* x-loader.bin.ift (you must rename this file to MLO) / This is a signed image using contrib/signGP tool
* igep.ini
* Your desired kernel image.

Don't use a uImage kernel format (from uboot), only kernel formats be supported.

Compilation Example:
$make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- zImage modules

Read the kernel documentation about kernel images.

4.2 Setup igep.ini file
------------------------
Please find a igep.ini example inside the scripts directory.
# Note this format permits use the characters
# '#' and ';' as comment check file size restrictions

[kernel]
kaddress=0x80008000
;rdaddress=0x81600000
serial.low=00000001
serial.high=00000000
revision=0001
;kImageName=
;kRdImageName=

[kparams]
console=ttyS2,115200n8
;earlyprintk=serial,ttyS2,115200
mem=512M
boot_delay=0
;mpurate=800
;loglevel=7
omapfb.mode=dvi:1024x768MR-16@60
smsc911x.mac=0xb2,0xb0,0x14,0xb5,0xcd,0xde
;ubi.mtd=2
;root=ubi0:igep0020-rootfs 
;rootfstype=ubifs
root=/dev/mmcblk0p2 rw rootwait

-----------------------------------

Tags Supported
---------------

---- [kernel] ----
* kaddress=0x80008000

Kernel copy address, you should use the same address used in kernel image
configuration. If you don't know what it means maybe it's better don't change it.

* rdaddress=0x81600000
Kernel RAM disk copy address. 
If you don't know what it means maybe it's better don't change it.

* serial.low=00000001
* serial.high=00000000
Board serial Number, you can read this information using /proc/cpuinfo

* revision=0001
Board Revision ID, you can read this information using /proc/cpuinfo

* kImageName=zImage
Kernel file name, if you don't provide this tag it try to load these others:
// DEFAULT IMAGES
"zImage"
"zimage"
"vmlinuz"
"bzImage"
"bzimage"

* kRdImageName=rdimage
Kernel RAM Disk file, if you don't provide this tag it try to load these others:
// DEFAULT IMAGES
"initrd"

* MachineID=id
Board ID actually support two values:
IGEP0020_MACHINE_ID             2344
IGEP0030_MACHINE_ID             2717


---- [kparams] ----
Kernel parameters, all these parameters are passed directly to the kernel using the
kernel command line.

kernel parameters documentation:
http://www.kernel.org/doc/Documentation/kernel-parameters.txt
http://www.kernel.org/pub/linux/kernel/people/gregkh/lkn/lkn_pdf/ch09.pdf

4.3 Boot Priority
-----------------
First try mmc and if it fails then try from OneNand. 

Examples:
a) MLO (x-loader), igep.ini, zImage from MMC
If all it's present in the mmc it don't try to boot from Onenand.

b) MLO (x-loader) in MMC, igep.ini and zImage in Onenand.
If only MLO it's provided this one try to load the other information from
the Onenand.

4.4 OneNand Partition settings
-------------------------------
We suggest use minimum 3 partitions on the OneNand.

Creating 3 MTD partitions on "omap2-onenand":
0x000000000000-0x000000080000 : "X-Loader"
0x000000080000-0x000000c80000 : "Boot"
0x000000c80000-0x000020000000 : "File System"

4.4.1) Xloader partition
* Not fs formated (raw)
* Suggested size: 0x80000 (512 KiB)
* The xloader must be signed before copy it in the flash memory.

You should copy the x-loader in the firsts 4 blocks (first 512 KiB), this is not a 
formated partition due the ROM not permits boot from there, you should use tools:
flash_eraseall and nandwrite for copy x-loader in the first blocks.

Suggested procedure:

nand_eraseall /dev/mtd0
nandwrite -p /dev/mtd0 <x-loader>

* Sign x-loader
You should execute contrib/signGP for sign the xloader that resides inside the flash memory.

contrib/signGP x-load.bin 
The signed x-loader it's named: x-load.bin.ift

Due the Onenand 512 MiB has two dies it's necessary split the x-loader and convert it to a 1 die binary.
This is a know OMAP/DM/AM OneNand/Nand boot limitation.

This is the procedure for create the x-loader OneNand binary:
You should execute: (You can use copy paste in your console)

split -b 2K x-load.bin.ift split-
for file in `ls split-a?`; do cat $file >> x-load-ddp.bin.ift; cat $file >> x-load-ddp.bin.ift; done

This last command generate a file named x-load-ddp.bin.ift this is the x-loader for copy it in the OneNand.

4.4.2 Boot Partition
--------------------
* fs used jffs2 zlib compressed filesystem.
* Suggested size: 0xC00000 (12 MiB)

First time creation:
a) Use the same procedure described in point 4.2.1. Copy your jffs2 compressed image in the
partition, it can be a empty file.

b) Erase the partition and mount it as jffs2 filesystem then you can copy with cp command.

Next Times:
Copy the files using cp command, or edit directly.

when kernel boots you can enable mount this partition over /boot directory for access all boot content.

4.4.3 Rootfs 
------------
* fs (your prefered fs supported by linux, maybe a good choice it should be ubifs)
* Size, all or you can create more partitions if you wish ... :)


5 Build procedure
=================

5.1 Build with Ubuntu Cross Compiler gcc 4.5.1

* This is tested with Ubuntu 10.10

a) Install the cross compiler:
apt-get install cpp-4.5-arm-linux-gnueabi g++-4.5-arm-linux-gnueabi 

b) Configure the board
make igep0020-sdcard_config

c) Build
make

d) Sign x-loader
You should execute contrib/signGP for sign the xloader that resides inside the flash memory.
contrib/signGP x-load.bin 
The signed x-loader it's named: x-load.bin.ift


5.2 Build with IGEP SDK

a) Source the enviroment
source /usr/local/poky/eabi-glibc/environment-setup-arm-none-linux-gnueabi 

b) Edit the file Makefile
Find the define:

And Set the variable as:
CROSS_COMPILE = arm-none-linux-gnueabi-

b) Configure the board
make igep0020-sdcard_config

c) build
make

d) Sign x-loader
You should execute contrib/signGP for sign the xloader that resides inside the flash memory.
contrib/signGP x-load.bin 
The signed x-loader it's named: x-load.bin.ift


5.3 Build Native

a) Configure the board
make igep0020-sdcard_config

b) Modify the config.mk file
Edit the variable CFLAGS and add the option: -fno-stack-protector

CFLAGS := $(CPPFLAGS) -Wall -Wstrict-prototypes -fno-stack-protector

c) build 
make CROSS_COMPILE=""

d) Sign x-loader
You should execute contrib/signGP for sign the xloader that resides inside the flash memory.
contrib/signGP x-load.bin 
The signed x-loader it's named: x-load.bin.ift


6 Contribution & Support & Report Bugs
======================================
Contributions to this project be welcome and you can send your patches to support@iseebcn.com
or you can use the igep forum for it.
You can access to IGEP-x-Loader repository using our git at git.igep.es
IGEP IRC Channel: http://webchat.freenode.net/?channels=igep
