SRC_DIR := src

DESTDIR ?= /tmp/portwell-lcd

default:
	gcc -O2 -I$(SRC_DIR) $(SRC_DIR)/eizo.c -o eizo

clean:
	rm -f eizo $(SRC_DIR)/*.o

install:
	mkdir -p $(DEFAULT)/usr/sbin
	cp eizo $(DESTDIR)/usr/sbin/
	cp -r -a files/* $(DESTDIR)/
