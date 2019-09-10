CC		=  gcc
CFLAGS	+= -std=c99 -Wall -Werror -pedantic -O3
LIBS    = -lpthread
OPTIONS= -D_POSIX_C_SOURCE=200809L
TARGETS = object_store libaccess.a test_client

.PHONY: all clean test

all		: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) $(OPTIONS) -c -o $@ $<

libaccess.a: access_library.o utils.o
	$(AR) rcvs $@ $^

object_store: object_store.c signal_handler.o connection_handler.o server_worker.o stats.o utils.o
	$(CC) $(CFLAGS) $(OPTIONS) -o $@ $^ $(LIBS)

connection_handler.o: server_worker.o utils.o

test_client: test_client.c libaccess.a
	$(CC) $(CFLAGS) $(OPTIONS) -o $@ $< -L. -laccess

clean		: 
	rm -f $(TARGETS)
	\rm -f *.o *~ *.a *.log

test: all
	@./test.sh > testout.log