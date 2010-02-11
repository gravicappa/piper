exe = piper
LDFLAGS += -lutil

all: ${exe}

${exe} : piper.c
	${CC} ${CFLAGS} $< ${LDFLAGS} -o $@ 
