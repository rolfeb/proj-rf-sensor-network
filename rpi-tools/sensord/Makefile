# vi: noexpandtab shiftwidth=8 softtabstop=8

WARN	= -Wall -Werror
LANG	= -std=c99 -fno-strict-aliasing -Wstrict-prototypes
IFLAGS	= -I../../include

CFLAGS	= $(LANG) $(WARN) -g
# CFLAGS	= $(LANG) $(WARN) -O2

sensord	:	sensord.c
	gcc $(IFLAGS) $(CFLAGS) -o $@ sensord.c -lmysqlclient

clean	:
	rm -f sensord
