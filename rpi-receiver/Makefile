# vi: noexpandtab shiftwidth=8 softtabstop=8

AVR_ROOT	=	../../avr-common

NAME		=	rpi-receiver

#
# Microcontroller definitions
#
MCU		=	atmega48p
###MCU		=	atmega644

#
# 10MHz clock
#
# LFUSE = 0xe7
#	CKDIV8		1	no divide by 8
#	CKOUT		1	clock output off
#	SUT1:0		10	xtal osc, fast rising power
#	CKSEL3:0	0111	xtal osc full swing
CPU_FREQ	=	10000000
HFUSE		=	$(DEFAULT_HFUSE)
LFUSE		=	0xe7

#
# 1.832MHz clock
#
### CPU_FREQ	=	1843200
### HFUSE		=	$(DEFAULT_HFUSE)
### LFUSE		=	0xeb

#
# Required application components
#
MODULES		=

#
# Application code
#
CFILES		=	\
			clock_1sec.c \
			wireless.c \
			main.c

#
# Load standard rules
#
include $(AVR_ROOT)/build/avr-build.mk

AVRDUDE = $(AVRDUDE_JTAGISP)
