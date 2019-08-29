CC		=  gcc
CFLAGS	+= -ggdb -std=c99 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Werror -pedantic -O0
LIBS    = -lpthread
INCLUDES	= -I. -D_POSIX_C_SOURCE=200809L
LDFLAGS 	= -L. -lasan -lubsan
TARGETS = object_store test_client


.PHONY: all clean cleanall
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)

object_store: object_store.c signal_handler.o connection_handler.o server_worker.o utils.o
	$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

connection_handler.o: server_worker.o utils.o

test_client: test_client.c access_library.o utils.o
	$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)



clean		: 
	rm -f $(TARGETS)
cleanall	: clean
	\rm -f *.o *~ *.a