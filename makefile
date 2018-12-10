CC=gcc
CFLAGS=-g -Wall -fPIC `pkgconf --cflags libpjproject`
LDFLAGS=-shared -fPIC `pkgconf --libs libpjproject`
TESTCFLAGS=-g -Wall `pkgconf --cflags libpjproject`
TESTLDFLAGS=`pkgconf --libs libpjproject`

all: voipms.so test_history.o test

config.h:
	@echo
	@echo 'first run `cp config.h.orig config.h` and edit the values'
	@echo
	@exit 1

voipms.so: voipms.o buffers.o sip_client.o constify.o history.o
	$(CC) $(LDFLAGS) -o $@ $^

voipms.o: voipms.c voipms.h buffers.h sip_client.h config.h
	$(CC) $(CFLAGS) -o $@ -c $<

buffers.o: buffers.c buffers.h voipms.h config.h
	$(CC) $(CFLAGS) -o $@ -c $<

sip_client.o: sip_client.c sip_client.h voipms.h constify.h config.h
	$(CC) $(CFLAGS) -o $@ -c $<

constify.o:constify.c constify.h
	$(CC) $(CFLAGS) -Wno-discarded-qualifiers -o $@ -c $<

history.o:history.c history.h
	$(CC) $(CFLAGS) -o $@ -c $<

## Testing

test_history.o:history.c history.h
	$(CC) $(CFLAGS) -o $@ -c $<

test:test.c test_history.o
	$(CC) $(TESTCFLAGS) $^ $(TESTLDFLAGS) -o $@

clean:
	rm -f *.o voipms.so test

install: voipms.so
	cp voipms.so $(HOME)/.weechat/plugins
