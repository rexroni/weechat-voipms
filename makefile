CC=gcc
CFLAGS=-Wall -fPIC `pkgconf --cflags libpjproject`
LDFLAGS=-shared -fPIC `pkgconf --libs libpjproject`

all: voipms.so

config.h:
	@echo
	@echo 'first run `cp config.h.orig config.h` and edit the values'
	@echo
	@exit 1

voipms.so: voipms.o buffers.o sip_client.o constify.o
	$(CC) $(LDFLAGS) $^ -o $@

voipms.o: voipms.c voipms.h buffers.h sip_client.h config.h
	$(CC) $(CFLAGS) -c $<

buffers.o: buffers.c buffers.h voipms.h
	$(CC) $(CFLAGS) -c $<

sip_client.o: sip_client.c sip_client.h voipms.h constify.h
	$(CC) $(CFLAGS) -c $<

constify.o:constify.c constify.h
	$(CC) $(CFLAGS) -Wno-discarded-qualifiers -c $<

%.o:%.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o voipms.so
