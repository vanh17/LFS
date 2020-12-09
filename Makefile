.PHONY: clean

clean:
	rm -f *.o *~ core sr *.dump *.tar tags mklfs lfs file lfsck

CC = gcc

CFLAGS = -g -Wall -std=gnu99 -D_DEBUG_

FUSE = `pkg-config fuse --cflags --libs` 

sourceMklfs = mklfs.c flash.c

sourceLfsck = fsmeta.c file.c log.c flash.c lfsck.c

sourceLfs = fsmeta.c log.c flash.c lfs.c file.c cleaner.c directory.c

lfs : $(sourceLfs)
	$(CC) $(CFLAGS) $(sourceLfs) $(FUSE) -o lfs

lfsck : $(sourceLfs)
	$(CC) $(CFLAGS) $(FUSE) -o lfsck $(sourceLfsck)

mklfs : $(sourceMklfs)
	$(CC) $(CFLAGS) $(FUSE) -o mklfs $(sourceMklfs) 