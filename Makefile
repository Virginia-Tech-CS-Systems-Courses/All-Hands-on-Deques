CFLAGS = -O0 -g -std=gnu99 -Wall
LDFLAGS = -lpthread

#programs =  test-spinlock-fifo test-lockfree-fifo test-waitfree-fifo
programs = test-deq-lock
all: $(programs)


test-deq-lock: test-spinlock.c
	$(CC) $(CFLAGS) -DTTAS $^ -o $@ $(LDFLAGS)

#test-lockfree-fifo: test-spinlock.c
#	$(CC) $(CFLAGS) -DLCKFREE_COPY $^ -o $@ $(LDFLAGS)

#test-waitfree-fifo: test-spinlock.c
#	$(CC) $(CFLAGS) -DWAIT_FREE $^ -o $@ $(LDFLAGS)



%:%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm -f *.o
	-rm -f $(programs)
