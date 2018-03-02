SRC_DIR := src

DESTDIR ?= /tmp/portwell-lcd

default:
	gcc -O2 -I$(SRC_DIR) $(SRC_DIR)/ezio.c -o ezio

clean:
	rm -f ezio $(SRC_DIR)/*.o

install:
	mkdir -p $(DESTDIR)/usr/sbin
	cp ezio $(DESTDIR)/usr/sbin/
	cp -r -a files/* $(DESTDIR)/
