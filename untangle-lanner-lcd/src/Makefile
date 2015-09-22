CC    := gcc

default:
ifeq ($(wildcard /dev/plcm_drv),)
	mknod /dev/plcm_drv c 248 0
endif
	gcc -O2 lanner-lcd.c -o lanner-lcd 
clean:
	rm -f lanner-lcd

install:
	cp -f lanner-lcd /usr/sbin/.
