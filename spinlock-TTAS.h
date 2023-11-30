#ifndef _SPINLOCK_XCHG_H
#define _SPINLOCK_XCHG_H

/* Spin lock using xchg.
 * Copied from http://locklessinc.com/articles/locks/
 */

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

static inline unsigned short xchg_8(void *ptr, unsigned char x)
{
    __asm__ __volatile__("xchgb %0,%1"
                :"=r" (x)
                :"m" (*(volatile unsigned char *)ptr), "0" (x)
                :"memory");

    return x;
}

#define BUSY 1
typedef unsigned char spinlock;

#define SPINLOCK_INITIALIZER 0

static inline void spin_lock_ttas(spinlock *lock)
{
    int backoff = 1;
    //printf("in the lock function");
    while (1) {
        if (0 == *lock && !xchg_8(lock, BUSY)) return;
    
        while (*lock) {
            for(int i = 0; i< backoff; i++){
                cpu_relax();
            }
            backoff = backoff*2;
            if(backoff >1024)
            backoff = 1024;
        }
    }
}

static inline void spin_unlock_ttas(spinlock *lock)
{
    barrier();
    *lock = 0;
}

static inline int spin_tryloc_ttas(spinlock *lock)
{
    return xchg_8(lock, BUSY);
}

#endif /* _SPINLOCK_XCHG_H */
