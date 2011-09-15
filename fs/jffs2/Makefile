#
# (C) Copyright 2000-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

TOPDIR=../../

include $(TOPDIR)/config.mk

LIB	= libjffs2.a

AOBJS	=
#ifdef CONFIG_CMD_JFFS2
COBJS-y += compr_lzo.o
#COBJS-y += compr_rtime.o
#COBJS-y += compr_rubin.o
COBJS-y += compr_zlib.o
COBJS-y += jffs2_1pass.o
COBJS-y += mini_inflate.o
#endif

COBJS	:= $(COBJS-y)
SRCS	:= $(AOBJS:.o=.S) $(COBJS:.o=.c)
OBJS	:= $(addprefix $(obj),$(AOBJS) $(COBJS))

#CPPFLAGS +=

all:	$(LIB) $(AOBJS)

$(LIB):	$(obj).depend $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)


#########################################################################

# defines $(obj).depend target
#include $(SRCTREE)/rules.mk
.depend:	Makefile $(AOBJS:.o=.S) $(COBJS:.o=.c)
		$(CC) -M $(CFLAGS) $(AOBJS:.o=.S) $(COBJS:.o=.c) > $@


sinclude $(obj).depend

#########################################################################


