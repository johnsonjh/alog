all: alog

clean:
	rm -f a.out alog

alog: alog.c
	$(CC) $(CFLAGS) $(LDFLAGS) alog.c -o alog

.PHONY: clean all
