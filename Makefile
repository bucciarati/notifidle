#!/usr/bin/make -f

CC := $(shell which colorgcc || which cc)

# change -O and -g in the end
CFLAGS += -O0 -ggdb -W -Wall -Wextra -std=c99 -pedantic

LDFLAGS += -lnotify

# ... WTF?
CPPFLAGS += -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/atk-1.0

.PHONY: all clean

all: notifidle

clean:
	$(RM) notifidle
