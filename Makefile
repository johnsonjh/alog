.PHONY: all
all: alog

alog: alog.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f alog
