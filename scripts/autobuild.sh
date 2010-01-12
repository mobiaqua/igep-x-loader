#!/bin/bash
#
# autobuild.sh : Automated Build Server Enviroment Setup
#
# Copyright (C) 2009 Integration Software and Electronics Engineering
# Author: Enric Balletbo i Serra <eballetbo@iseebcn.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

# XLOADER variables
XLOADER_BOARD="igep0020b"
XLOADER_MEDIA="sdcard flash"
SIGNGP="contrib/signGP"

# Fancy output functions
log_use_fancy_output () {
    TPUT=/usr/bin/tput
    EXPR=/usr/bin/expr
    if [ "x$TERM" != "xdumb" ] && [ -x $TPUT ] && [ -x $EXPR ] && $TPUT hpa 60 >/dev/null 2>&1 && $TPUT setaf 1 >/dev/null 2>&1; then
        [ -z $FANCYTTY ] && FANCYTTY=1 || true
    else
        FANCYTTY=0
    fi
    case "$FANCYTTY" in
        1|Y|yes|true)   true;;
        *)              false;;
    esac
}

log_pass_msg () {
    if log_use_fancy_output; then
				NORMAL=`$TPUT op`
				GREEN=`$TPUT setaf 2`
        /bin/echo -e "  - ${GREEN}(PASS)${NORMAL} $@"
    else
        /bin/echo -e "  - (PASS) $@"
    fi
}

log_fail_msg () {
    if log_use_fancy_output; then
				NORMAL=`$TPUT op`
				RED=`$TPUT setaf 1`
        /bin/echo -e "  - ${RED}(FAIL)${NORMAL} $@"
    else
        /bin/echo -e "  - (FAIL) $@"
    fi
}

log_skip_msg () {
    if log_use_fancy_output; then
				NORMAL=`$TPUT op`
				YELLOW=`$TPUT setaf 3`
        /bin/echo -e "  - ${YELLOW}(SKIP)${NORMAL} $@"
    else
        /bin/echo -e "  - (SKIP) $@"
    fi
}

# Setup environment
MAKE=make
TARGETDIR=./autobuild

# Check for proper number of command line args.
EXPECTED_ARGS=1

if [ $# -ne $EXPECTED_ARGS ]
then
  echo "Usage: ${0} [cross_compile_prefix]"
	echo "Example:"
  echo "  ${0} arm-none-linux-gnueabi-"
  exit 1
fi

# Prepare autobuild environment
if [ ! -d "${TARGETDIR}" ]; then
  mkdir -p ${TARGETDIR}
fi
rm -rf ${TARGETDIR}/*

# Build
for board in ${XLOADER_BOARD}; do
	mkdir ${TARGETDIR}/${board}
	> ${TARGETDIR}/${board}/autobuild.log
	echo "Building x-loader for ${board} : "

	for media in ${XLOADER_MEDIA}; do
		${MAKE} CROSS_COMPILE=${1} distclean                >> ${TARGETDIR}/${board}/autobuild.log 2>&1
		${MAKE} CROSS_COMPILE=${1} ${board}-${media}_config >> ${TARGETDIR}/${board}/autobuild.log 2>&1
		${MAKE} CROSS_COMPILE=${1}                          >> ${TARGETDIR}/${board}/autobuild.log 2>&1
		if [ "$?" -ne 0 ]; then
			log_fail_msg "${media}"
		else
			log_pass_msg "${media}"
			mkdir -p ${TARGETDIR}/${board}/${media}
			cp x-load.bin ${TARGETDIR}/${board}/${media}
			${SIGNGP} ${TARGETDIR}/${board}/${media}/x-load.bin
			cp x-load.bin ${TARGETDIR}/${board}/${media}
		fi
		${MAKE} CROSS_COMPILE=${1} distclean >/dev/null 2>&1
	done
done

exit 0
