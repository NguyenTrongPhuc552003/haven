# Haven guest userspace CLI
CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -I../../include -I.

all: haven_tool

haven_tool: haven_tool.c haven_ioctl.h
	$(CC) $(CFLAGS) -o haven_tool haven_tool.c

clean:
	rm -f haven_tool

.PHONY: all clean
