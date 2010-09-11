dst = piper editwrap
LDFLAGS += -lutil
destdir=/usr/local

all: $(dst)

install:
	install -m 755 $(dst) $(destdir)/bin

clean:
	-rm $(dst) 2>/dev/null

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
