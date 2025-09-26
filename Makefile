.PHONY: all
all: alog

alog: alog.o alog_util.o endian.o
	$(CC) $(CFLAGS) $(LDFLAGS) alog.o alog_util.o endian.o -o alog

.PHONY: clean
clean:
	rm -f endian.o alog_util.o alog.o alog

endian.o: endian.c endian.h
alog_util.o: alog_util.c alog.h endian.h
alog.o: alog.c alog.h endian.h
