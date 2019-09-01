CC		=  gcc
CFLAGS	+= -std=c99 -Wall -Werror -pedantic -O3
LIBS    = -lpthread
INCLUDES	= -I. -D_POSIX_C_SOURCE=200809L
LDFLAGS 	= -L.
TARGETS = object_store test_client


.PHONY: all clean test
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)

object_store: object_store.c signal_handler.o connection_handler.o server_worker.o stats.o utils.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

connection_handler.o: server_worker.o utils.o

test_client: test_client.c access_library.o utils.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)



clean		: 
	rm -f $(TARGETS)
	\rm -f *.o *~ *.a

test: all
	@./object_store &
	@./test.sh > testout.log