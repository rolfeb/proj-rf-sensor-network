# vi: noexpandtab shiftwidth=8 softtabstop=8

AVR_ROOT	=	../../avr-common

NAME		=	sensor-t

#
## Microcontroller definitions
#
MCU             =       attiny25

#
# 3.6864 MHz clock
#
# LFUSE = 0xfb
#	CKDIV8		1	no divide by 8
#	CKOUT		1	clock output off
#	SUT1:0		10	xtal osc, fast rising power
#	CKSEL3:1	110	xtal osc, 3-8 MHZ
#	CKSEL0		1	xtal osc, fast rising power
#
CPU_FREQ	=	3686400
HFUSE		=	$(DEFAULT_HFUSE)
LFUSE		=	0xed

#
# Required application components
#
MODULES         =	\
			ds1820 \
			one-wire

#
# Application code
#
CFILES		=	\
			sensor.c
#
# Load standard rules
#
include $(AVR_ROOT)/build/avr-build.mk
