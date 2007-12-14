SOURCE := generator.c
OBJECTS := $(SOURCE:.c=.o)
BINARY := mvverifykey
LDFLAGS := -lssl
CFLAGS := -c -Wall -DMV_VERIFY_KEY_ONLY
DESTDIR := /usr/bin
INSTALL_PROGRAM := /usr/bin/install

all: install
install: $(BINARY)
	$(INSTALL_PROGRAM) $< $(DESTDIR)/

$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $< -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(BINARY)

.PHONY: clean install