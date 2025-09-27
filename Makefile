.PHONY: all
all: alog

alog: alog.c
	$(CC) $(CFLAGS) $(LDFLAGS) alog.c -o alog

.PHONY: clean
clean:
	rm -f a.out alog
