ARCH := $(shell dpkg-architecture -qDEB_BUILD_ARCH)

ifeq ($(ARCH),amd64)
  SPAMCATDDIR := ./64
else
  SPAMCATDDIR := ./32
endif

DESTDIR ?= /tmp/spamcatd
TMPDIR := ./tmp

all: build

build:
	mkdir -p $(TMPDIR)
	for tarball in $(SPAMCATDDIR)/spamcatd*.tar.gz ; do \
	  tar -C $(TMPDIR) -xavf $$tarball ; \
	done

	for tarball in $(SPAMCATDDIR)/spamcatcher*.tar.gz ; do \
	  mkdir -p $(TMPDIR)/spamcatchersdk ; \
	  tar -C $(TMPDIR)/spamcatchersdk --strip-components=1 -xavf $$tarball ; \
	done
	cp -a $(TMPDIR)/spamcatchersdk/lib/* $(TMPDIR)/spamcatd/lib/

	make -C $(TMPDIR)/spamcatd all

clean:
	make -C $(TMPDIR)/spamcatd clean || true
	rm -fr $(TMPDIR)

install: build
	mkdir -p $(DESTDIR)/usr/bin $(DESTDIR)/etc/spamcatd $(DESTDIR)/usr/share/perl5/Mail/SpamAssassin/Plugin
	mkdir -p $(DESTDIR)/etc/spamassassin $(DESTDIR)/usr/lib/spamcatd

	cp -r -a files/* $(DESTDIR)

	cp $(TMPDIR)/spamcatd/spamcatd $(DESTDIR)/usr/bin/spamcatd
	chmod 755 $(DESTDIR)/usr/bin/*

	cp $(TMPDIR)/spamcatd/*.conf $(DESTDIR)/etc/spamcatd
	cp $(TMPDIR)/spamcatd/conf/*.conf $(DESTDIR)/etc/spamcatd
	cp -a $(TMPDIR)/spamcatchersdk/lib/libspamcatcher.so* $(DESTDIR)/usr/lib/spamcatd/

	cp ./plugin/MailShell.pm $(DESTDIR)/usr/share/perl5/Mail/SpamAssassin/Plugin
	cp ./plugin/mailshell.pre $(DESTDIR)/etc/spamassassin
	cp ./plugin/mailshell.cf $(DESTDIR)/etc/spamassassin
