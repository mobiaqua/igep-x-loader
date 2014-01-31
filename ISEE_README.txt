#
# (C) Copyright 2009-2012 ISEE
# Manel Caro (mcaro@iseebcn.com)
#
# Version: IGEP-X-Loader 2.5.0-2
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
4.4 Nand Partition settings
4.4.1 Xloader partition
4.4.2 Boot Partition
4.4.3 Rootfs
5 Build procedure
5.1 Build with Ubuntu Cross Compiler gcc 4.5.1
5.2 Build Native
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
* Added fs jffs2 support using mtd & onenand/nand support (Read Only).
* Added crc32, zlib, lzo. 
* Jffs2 zlib & lzo compression support (Read Only).
* Dual boot mmc & onenand with mmc highest priority.
* Added Linux kernel boot directly (Support for 2.6.22 and highest version kernels)
* Linux kernel supported images: vmlinuz, bzImage and zImage.
* Support for loading Linux Ram disk
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
* Support gcc 4.5.x linaro version
* Added Support Initial Ram disk
* Reconfigure Makefile options
* Support Kernels 2.6.35 and 2.6.37
* Added support for boot a ARM binary executable
* Support for Numonyx, Micron & hynix POP memories
* Memory Autodetection
* Added DMA Copy Support
* Added NAND async driver Support
* Change memcpy function
* Support ISEE toolchain yocto 1.2
* Added DSS Video Driver
* Added DSS igep.ini variables

[NEW in this Version]

* Change MPU boot up speed

2.2 Issues & Limitations
------------------------

* The ini file it's limited to max size: 16 KiB
  This is not a real limitation due all ini file it's 
  copied into the RAM memory.
* Kernel Command line parameters it's limited to: 4 KiB
  This is not a real limitation due all ini file it's 
  copied into the RAM memory.
* Malloc it's limited to 32 MiB (this is not a real issue)
* Video Memory it's limited to 1024 x 768 x 4

2.3 TODO
--------

* Remove compilation warnings.
* Improve boot selection mode and priority.

2.5 Version Changes
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
[2.3.0-1] Add NAND flash devices and Micron MT29CXGXXMAXX memories support
[2.3.0-2] Add Hynix NAND memorie and IGEP0032 support
----
[2.3.0-3] Add Support for execute ARM binaries
[2.3.0-3] Bug Fixes related to I and D Cache
----
[2.4.0-1] Added Memory test feature
[2.4.0-1] Added some boot information
[2.4.0-1] New read_nand_cache function optimized for load from NAND
[2.4.0-1] BUG resolved: Refresh Setup in Micron & Hynix Memories
[2.4.0-1] BUG resolved: Reset Memory controller after initialize Malloc function
[2.4.0-1] BUG resolved: Resolve problems updating the flash content under jffs2
----
[2.4.0-2] Resolved Memory Autodetection
[2.4.0-2] Better hang board led control
[2.4.0-2] New read_nand_cache function optimized for load from OneNand
[2.4.0-2] Added Hw GPtimer functionality
----
[2.5.0-1] Added DMA driver
[2.5.0-1] Added Memcpy optimized function
[2.5.0-1] Improved boot speed
[2.5.0-1] Improve Nand driver support Micron & Hynix Memories
[2.5.0-1] x-load.bin.ift and MLO generation 
[2.5.0-1] Added DSS Video Driver 
----
[2.5.0-2] Fix DMA driver misconfiguration
[2.5.0-2] Improve GPMC timming setup
[2.5.0-2] Added igep.ini DSS configuration Variables
[2.5.0-2] Added support for new Micron & Hynix Memories
[2.5.0-2] Bug fixes  
----
[2.5.0-3] platform.S: downgrade MPU boot clock from 1GHz to 800 MHz
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
this howto: http://labs.isee.biz/index.php/How_to_boot_from_MicroSD_Card)
In this first partition (boot partition) you should copy:

* x-loader.bin.ift (you must rename this file to MLO) / This is a signed image using contrib/signGP tool
* igep.ini
* Your desired kernel image.

Don't use a uImage kernel format (from uboot), only kernel formats be supported.

Kernel Compilation Example:
$make ARCH=arm CROSS_COMPILE=arm-poky-linux-gnueabi- zImage modules

Read the kernel documentation about kernel images.

4.2 Setup igep.ini file
------------------------
Please find a igep.ini example inside the scripts directory.
# Note this format permits use the characters
# '#' and ';' as comment check file size restrictions

[kernel]
; Kernel load address, NOT Modify
kaddress=0x80008000
; RAM disk load Address, NOT Modify (Uncomment if you wish load a ram disk)
;rdaddress=0x81600000
; Board Serial ID
serial.low=00000001
serial.high=00000000
; Board Revision
revision=0003
; Kernel Image Name or ARM binary file
kImageName=zImage
;kImageName=u-boot.bin
; Kernel RAM Disk Image Name (Uncomment with your ramdisk file)
;kRdImageName=initrd.img-2.6.35-1010-linaro-omap
;kRdImageName=rd-igepv2.bin
; Module = 2717 IGEPv2 = 2344
MachineID=2344
; Boot Mode = binary or kernel
Mode=kernel
; DSS Video Activation (0 disbale - !=0 enable)
dss=1
; DSS color, enabled if not bitmap is used and dss=1 (hex data)
dss_color=0x00FF8000
; DSS Bitmap image filename (string)
dss_bitmap=splash.dat

[kparams]
; IGEP configuration set
;buddy=base0010
buddy=igep0022
; Setup the Kernel console params
; ttyO2 = if kernel it's >= 2.6.37
console=ttyO2,115200n8
; ttyO2 = if kernel it's <= 2.6.35
;console=ttyS2,115200n8
; Enable early printk
;earlyprintk=serial,tty02,115200
;earlyprintk=serial,ttyS2,115200
; Setup the Board Memory Configuration
mem=430M
;mem=512M
;mem=64M
;mem=128M
; Setup the Boot Delay
boot_delay=0
; Setup the ARM Processor Speed
;mpurate=1000
; Setup the loglevel
;loglevel=7
;Enable Kernel Debug Output
;debug=1
;Fix RTC Variable
;fixrtc=1
;Configure nocompcache variable
nocompcache=1
; Configure Frame Buffer Configuration
;omapfb.mode=dvi:1280x720MR-16@60
omapfb.mode=dvi:hd720-16@50
;Configure Video Ram assigned
;vram=40M
;vram=4M
; Configure Video RAM assigned to every frame buffer
;omapfb.vram=0:12M,1:16M,2:12M
; Configure frame buffer debug output
;omapfb.debug=1
; Configure DSS Video Debug option
;omapdss.debug=1
; Configure the Board Ethernet Mac Address
smsc911x.mac0=0xb2,0xb0,0x14,0xb5,0xcd,0xde
smsc911x.mac1=0xb2,0xb0,0x14,0xb5,0xcd,0xdf
smsc911x.mac=0xb2,0xb0,0x14,0xb5,0xcd,0xde
;  --- Configure UBI FS boot --- 
;ubi.mtd=2 
;root=ubi0:igep0020-rootfs 
;rootfstype=ubifs
;  --- Configure NFS boot --- 
;ip=192.168.2.123:192.168.2.110:192.168.2.1:255.255.255.0::eth0:
;root=/dev/nfs
;nfsroot=192.168.2.110:/srv/nfs/linaro_netbook
;  --- Configure MMC boot --- 
root=/dev/mmcblk0p2 rw rootwait
;root=/dev/ram0 rw
; Assign Init program
;init=/bin/bash
;init=/linuxrc

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
IGEP0032_MACHINE_ID				3203

* Mode=<string>
kernel : Boot a Linux kernel 
binary : Boot ARM binary

* dss=<boolean (0 or 1)>
0 : Disbale DSS Video
1 : Enable DSS Video

* dss_color=<hex value>
0x00FF8000

* dss_bitmap=<string>
bitmap file name.

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

4.4 Nand Partition settings
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
flash_eraseall and writeloader for copy x-loader in the first blocks.

Suggested procedure:

$ flash_eraseall /dev/mtd0
$ writeloader -i <x-load.bin.ift> -o /dev/mtd0

Where: x-load.bin.ift it's the bootloader signed file

4.4.2 Boot Partition
--------------------
* fs used jffs2 zlib or lzo compressed filesystem.
* Suggested size: 0xC00000 (12 MiB)

First time creation:

a) boot from a microsd card as described in point 4.1

b) Erase the boot partition
$ flash_eraseall /dev/mtd1

c) mount the partition
$ mount -t jffs2 /dev/mtdblock1 /mnt

d) Copy igep.ini and kernel image
You should copy minimum the igep.ini file and your desired kernel image as
cp igep.ini /mnt
cp zImage /mnt

When you need update or modify the contents you should mount the partition 
as point (c) and then edit or copy your desired files directly.

4.4.3 Rootfs 
------------
* fs (your prefered fs supported by linux, maybe a good choice it should be ubifs)
* Size, all or you can create more partitions if you wish ... :)


5 Build procedure
=================

5.1 Build with ISEE SDK Yocto toolchain 1.2.1.1

Download the right toolchain and install it in your host machine.

a) Setup the build enviroment:
source /opt/poky/1.2/environment-setup-armv7a-vfp-neon-poky-linux-gnueabi

b) Configure the board
make igep00x0_config

c) Build
make

The result will be two files:

* x-load.bin.ift
* MLO
You can use MLO for boot from a microsd card or flash.

5.2 Build Native

a) Configure the board
make igep00x0_config

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
You can access to IGEP-x-Loader repository using our git at git.isee.biz
IGEP IRC Channel: http://webchat.freenode.net/?channels=igep
BUG Tracking: http://bug.isee.biz
