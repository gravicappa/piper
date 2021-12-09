dst = piper stdio

LDFLAGS += -lutil
destdir=/usr/local
bindir=$(destdir)/bin
mandir=$(destdir)/share/man

-include config.mk

all: $(dst)

install:
	install -m 755 $(dst) $(bindir)
	install -m 644 piper.1 stdio.1 $(mandir)/man1

clean:
	-rm $(dst) 2>/dev/null

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
