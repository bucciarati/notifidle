#!/usr/bin/make -f

CC := $(shell which colorgcc || which cc)

# change -O and -g in the end
CFLAGS += -O0 -ggdb -W -Wall -Wextra

.PHONY: all clean

all: notifidle

clean:
	$(RM) notifidle
