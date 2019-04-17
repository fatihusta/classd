VERSION := 5.26.2

BCI_SDK_DIR := bcti-sdk-linux-${VERSION}
BCI_SDK_TAR := ./${BCI_SDK_DIR}.tgz

CONF_DIR=/etc/bctid
BIN_DIR=/usr/bin
LIB_DIR=/usr/lib/bctid
DATABASE_DIR=/var/lib/bctid/database

TMPDIR := ./tmp
DESTDIR ?= /tmp/bctid

all: build

build:
	rm -rf $(TMPDIR)
	mkdir -p $(TMPDIR)
	tar -C $(TMPDIR) -xaf ${BCI_SDK_TAR}

	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0001_tcp_ack_delay_fix.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0002_catid_option.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0003_client_elapsed_time.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0004_cache_status_and_clear.patch

	make -C $(TMPDIR)/${BCI_SDK_DIR}/src Daemon

install: build
	mkdir -p $(DESTDIR)${CONF_DIR}
	mkdir -p $(DESTDIR)${LIB_DIR}
	mkdir -p $(DESTDIR)${BIN_DIR}
	mkdir -p $(DESTDIR)${DATABASE_DIR}

	mkdir -p $(TMPDIR)
	cp $(TMPDIR)/${BCI_SDK_DIR}/bin/bctid $(DESTDIR)${BIN_DIR}
	cp $(TMPDIR)/${BCI_SDK_DIR}/lib/* $(DESTDIR)${LIB_DIR}
	cp $(TMPDIR)/${BCI_SDK_DIR}/database/* $(DESTDIR)${DATABASE_DIR}

clean:
	rm -fr $(TMPDIR)
