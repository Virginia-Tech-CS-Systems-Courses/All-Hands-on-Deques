CFLAGS = -O0 -g -std=gnu99 -Wall
LDFLAGS = -lpthread -latomic


programs =  test-deq-lock test-deq-lockfree
#programs = test-deq-lockfree 
all: $(programs)


test-deq-lock: test-spinlock.c
	$(CC) $(CFLAGS) -DTTAS $^ -o $@ $(LDFLAGS)

test-deq-lockfree: test-spinlock.c
	$(CC) $(CFLAGS) -DLCKFREE $^ -o $@ $(LDFLAGS)

#test-waitfree-fifo: test-spinlock.c
#	$(CC) $(CFLAGS) -DWAIT_FREE $^ -o $@ $(LDFLAGS)



%:%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm -f *.o
	-rm -f $(programs)
