#
# (C) Copyright 2009-2011 ISEE
# Manel Caro (mcaro@iseebcn.com)
#
# Change log:
# Version: ISEE - IGEP - X-Loader 2.0.0-1 
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

1) Summary:
===========
This directory contains the source code for X-Loader, an initial program
loader for Embedded boards based on OMAP processors.

2) Features and Issues:
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

2.2 Issues

* The ini file it's limited to max size: 16 KiB
  This is not a real limitation due all ini file it's 
  copied into the RAM memory.
* Kernel Command line parameters it's limited to: 4 KiB
  This is not a real limitation due all ini file it's 
  copied into the RAM memory.
* Malloc it's limited to 32 MiB.

2.3 TODO

* Add support for IGEP0020 - OMAP3530 Processor family.
* Add support for IGEP0020 - Processor under 1Ghz (600, 720 Mhz)
* Add support for IGEP0030 - Family boards.
* Add support for other OMAP/DM/AM processor boards.


3) Status:
==========

* Support IGEP0020 family boards (at this moment only DM3730 it's supported).
	- Tested with IGEPv2 (DM3730@1Ghz and 512/512 MB Ram/Onenand)
        - Tested with IGEPv2 (AM3703@1Ghz and 512/512 MB Ram/Onenand)

4) Settings & Configuration:
============================

4.1 MMC Boot
------------
Get a new mmc and create two partitions, the first one must be fat (you can follow 
this howto: http://code.google.com/p/beagleboard/wiki/LinuxBootDiskFormat)
In this first partition (boot partition) you should copy:

* x-loader.bin (you must rename this file to MLO)
* igep.ini
* Your desired kernel image.

Don't use a uImage kernel format (from uboot), only kernel formats be supported.

Compilation Example:
$make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- zImage modules

Read the kernel documentation about kernel images.

4.2 Setup igep.ini file
------------------------
# Note this format permits use the characters
# '#' and ';' as comment check file size restrictions

[kernel]
kaddress=0x80008000
;rdaddress=0x84000000
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

* rdaddress=0x84000000
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
* Not fs formated
* Suggested size: 0x80000 (512 KiB)
* The xloader must be signed before copy it in the flash memory.

You should copy the x-loader in the firsts 4 blocks (first 512 KiB), this is not a 
formated partition due the ROM not permits boot from there, you should use tools:
flash_eraseall and nandwrite for copy x-loader in the first blocks.

Suggested procedure:

nand_eraseall /dev/mtd0
nandwrite -p /dev/mtd0 <x-loader>

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






source /usr/local/poky/eabi-glibc/environment-setup-arm-none-linux-gnueabi 

Sign x-loader
--------------
contrib/signGP x-load.bin

---- split x-loader
split -b 2K x-load.bin.ift split-
for file in `ls split-a?`; do cat $file >> x-load-ddp.bin.ift; cat $file >> x-load-ddp.bin.ift; done


