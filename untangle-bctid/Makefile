VERSION := 5.27.2

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

	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0001_load_db.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0002_client_send_performance.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0006_host_colon.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0008_on_error.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0009_status.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0010_status_ip_error.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0011_ipgetinfo.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0012_resolve.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0013_restart.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0014_test_connection.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0015_heartbeat.patch
	patch -p1 -d $(TMPDIR)/${BCI_SDK_DIR} < ./0016_protocol_fail.patch

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
