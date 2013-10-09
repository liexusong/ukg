# create by Liexsuong <280259971@qq.com>

DEBUG?= -g
CFLAGS?= -std=c99 -pedantic -O2 -Wall -W -DSDS_ABORT_ON_OOM
CCOPT= $(CFLAGS)

OBJ = ukg.o ae.o
PRGNAME = ukg

all: server

server: $(OBJ)
	$(CC) -o $(PRGNAME) $(DEBUG) $(OBJ) $(CCOPT)

ukg.o: ukg.c
	$(CC) -c ukg.c

ae.o: ae.c ae.h
	$(CC) -c ae.c

clean:
	rm -rf $(PRGNAME) *.o
