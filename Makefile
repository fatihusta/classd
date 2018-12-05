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

ifneq (,$(findstring mvebu,$(CFLAGS)))
  ARCH := $(shell echo $(CFLAGS) | sed -e 's/.*-mcpu=\(.*\) .*/\1/')
  LIBDIR := $(SRC_DIR)/lib$(ARCH)-openwrt1806-libmusl
  PLUGDIR := $(SRC_DIR)/plugins$(ARCH)/
else # Debian
  CXXFLAGS += -pthread
  LIBFILES += -lpthread -ldl
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
	echo $(CFLAGS)
	env
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

