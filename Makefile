exe = piper
LDFLAGS += -lutil
destdir=/usr/local

all: ${exe}

install:
	install -m 755 ${exe} ${destdir}/bin

${exe} : piper.c
	${CC} ${CFLAGS} $< ${LDFLAGS} -o $@ 
