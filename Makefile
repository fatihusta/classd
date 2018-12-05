# Untangle Traffic Classification Daemon
# Copyright (c) 2011-2017 Untangle, Inc.
# All Rights Reserved
# Written by Michael A. Hotz

#DEBUG = -g3 -ggdb
#GPROF = -pg
SPEED = -O2

SRC_DIR := src/vineyard
DESTDIR ?= /tmp/vineyard

LIBFILES = -lnavl
PLATFORM = -D__LINUX__
CXXFLAGS += $(DEBUG) $(GPROF) $(SPEED) -Wall
CXXFLAGS += -pthread
LIBFILES += -lpthread -ldl

ifeq ($(OPENWRT_BUILD),1)
  ARCH := $(shell echo $(STAGING_DIR) | sed -e 's/.*target-\(.*\)_eabi/\1/')
  ifeq ($(ARCH),arm_cortex-a9+vfpv3_musl)
    LIBDIR := $(SRC_DIR)/libmvebu-openwrt1806-libmusl
    PLUGDIR := $(SRC_DIR)/pluginsmvebu-openwrt1806-libmusl
  else # assume x64+musl
    LIBDIR := $(SRC_DIR)/lib64-openwrt1806-libmusl
    PLUGDIR := $(SRC_DIR)/pluginslib64-openwrt1806-libmusl
  endif
else # Debian
  ARCH := $(shell dpkg-architecture -qDEB_BUILD_ARCH)
  ifeq ($(ARCH),amd64)
    LIBDIR := $(SRC_DIR)/lib64
    PLUGDIR := $(SRC_DIR)/plugins64/
  else ifeq ($(ARCH),armel)                          
    LIBDIR := $(SRC_DIR)/libarm
    PLUGDIR := $(SRC_DIR)/pluginsarm/
  else ifeq ($(ARCH),armhf)
    LIBDIR := $(SRC_DIR)/libarmhf
    PLUGDIR := $(SRC_DIR)/pluginsarmhf/
  else
    LIBDIR := $(SRC_DIR)/lib
    PLUGDIR := $(SRC_DIR)/plugins/
  endif
endif

BUILDID := "$(shell date -u '+%G/%m/%d %H:%M:%S UTC')"
VERSION := $(shell date -u "+%s")

CXXFLAGS += -DVERSION=\"$(VERSION)\"
CXXFLAGS += -DBUILDID=\"$(BUILDID)\"
CXXFLAGS += -DPLATFORM=\"$(PLATFORM)\"

OBJFILES := $(patsubst src/%.cpp,src/%.o,$(wildcard src/*.cpp))

classd : $(OBJFILES)
	$(CXX) $(OBJFILES) -L$(LIBDIR) $(LIBFILES) -o classd

install: classd
	mkdir -p $(DESTDIR)/usr/bin $(DESTDIR)/usr/lib $(DESTDIR)/usr/share/untangle-classd/plugins
	cp -a -r files/* $(DESTDIR)/
	cp -a classd $(DESTDIR)/usr/bin
	cp -d $(LIBDIR)/* $(DESTDIR)/usr/lib/
	find $(PLUGDIR) -name '*.TXT' | while read f ; do cp "$$f" $(DESTDIR)/usr/share/untangle-classd/plugins/ ; done

$(OBJFILES) : Makefile src/*.h

clean : force
	rm -f classd
	rm -f src/*.o

force :

