VERSION := 1.19

ARCH := $(shell dpkg-architecture -qDEB_BUILD_ARCH)
ifeq ($(ARCH),amd64)
  ARCH_GLIBC := x86_64-glibc23
else
  BDAMTAR := x86-glibc22
endif

BDAMTAR := ./clientserver-$(VERSION)-linux-$(ARCH_GLIBC).tar.gz

TMPDIR := ./tmp

DESTDIR ?= /tmp/bdamserver

all:

install:
	mkdir -p $(DESTDIR)/etc/bdamserver $(DESTDIR)/etc/spamassassin $(DESTDIR)/usr/lib/bdamserver
	mkdir -p $(DESTDIR)/usr/bin $(DESTDIR)/usr/share/perl5/Mail/SpamAssassin/Plugin
	mkdir -p $(TMPDIR)
	tar -C $(TMPDIR) --strip-components 1 -xavf $(BDAMTAR) 
	cp $(TMPDIR)/bin/bdamserver $(DESTDIR)/usr/lib/bdamserver
	cp $(TMPDIR)/bin/bdamclient $(DESTDIR)/usr/bin
	cp $(TMPDIR)/bin/*.so $(DESTDIR)/usr/lib/bdamserver
	cp $(TMPDIR)/lib/*.so $(DESTDIR)/usr/lib/
	cp $(TMPDIR)/etc/*.conf $(DESTDIR)/etc/bdamserver
	cp ./plugin/SpamBlocker.pm $(DESTDIR)/usr/share/perl5/Mail/SpamAssassin/Plugin
	cp ./plugin/spamblocker.pre $(DESTDIR)/etc/spamassassin
	cp ./plugin/spamblocker.cf $(DESTDIR)/etc/spamassassin

clean:
	rm -fr $(TMPDIR)
