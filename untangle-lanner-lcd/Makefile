SRC_DIR := src

DESTDIR ?= /tmp/lanner-lcd

default:
	gcc -O2 -I$(SRC_DIR) $(SRC_DIR)/lanner-lcd.c -o lanner-lcd 

clean:
	rm -f lanner-lcd $(SRC_DIR)/*.o

install:
	cp lanner-lcd $(DEFAULT)/usr/sbin/
	cp -r -a files/* $(DEFAULT)/
