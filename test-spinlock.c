#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>


#ifdef TTAS
#include "spinlock-TTAS.h"
#include "DEQ_Lock.h"
#include "DEQ_Lock_QSORT.h"
#elif defined(LCKFREE)
#include "DEQ_LockFree.h"
#include "DEQ_LockFree_Qsort.h"
#endif

#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory")
#endif

enum Test{
    Fibonacci = 0,
    QSORT
};



enum Test state = Fibonacci;

typedef struct QuicksortTaskArg {
    int* array;
    int low;
    int high;
} QuicksortTaskArg; 

/*
 * You need  to provide your own code for bidning threads to processors
 * Use lscpu on rlogin servers to get an idea of the number and topology
 * of processors. The bind_
 */

/* Number of total lock/unlock pair.
 * Note we need to ensure the total pair of lock and unlock opeartion are the
 * same no matter how many threads are used. */
#define N_PAIR 16000000

#define TOTAL_OPERATIONS 32
#define ARRAY_SIZE 100
int* Qsort_array;



/* Bind threads to specific cores. The goal is to make threads locate on the
 * same physical CPU. Modify bind_core before using this. */
//#define BIND_CORE

static int nthr = 0;



static volatile uint32_t test_counter = 0;
pthread_barrier_t barrier;

static volatile uint32_t wflag;
/* Wait on a flag to make all threads start almost at the same time. */
void wait_flag(volatile uint32_t *flag, uint32_t expect) {
    __sync_fetch_and_add((uint32_t *)flag, 1);
    while (*flag != expect) {
        cpu_relax();
    }
}

static struct timeval start_time;
static struct timeval end_time;
int op_counter = 0;

static void calc_time(struct timeval *start, struct timeval *end) {
    if (end->tv_usec < start->tv_usec) {
        end->tv_sec -= 1;
        end->tv_usec += 1000000;
    }

    assert(end->tv_sec >= start->tv_sec);
    assert(end->tv_usec >= start->tv_usec);
    struct timeval interval = {
        end->tv_sec - start->tv_sec,
        end->tv_usec - start->tv_usec
    };
    printf("Execution time is %ld.%06ld\n", (long)interval.tv_sec, (long)interval.tv_usec);
    //printf("test counter is %d \n",op_counter);
}

// Use an array of counter to see effect on RTM if touches more cache line.
#define NCOUNTER 1
#define CACHE_LINE 64

// Use thread local counter to avoid cache contention between cores.
// For TSX, this avoids TX conflicts so the performance overhead/improvement is
// due to TSX mechanism.
static int8_t counter[CACHE_LINE*NCOUNTER];

typedef struct ThreadInfo {
    int id;
    Deque* localDeque;
    int fibonacciPrev1;  // Previous Fibonacci value
    int fibonacciPrev2;  // Second previous Fibonacci value
} ThreadInfo;

typedef struct ThreadInfo_qs {
    int id;
    Deque_qs* localDeque;
} ThreadInfo_qs;



#ifdef TTAS
spinlock sl;
#else

#endif

#ifdef BIND_CORE
void bind_core(int threadid) {
    /* cores with logical id 2x   are on node 0 */
    /* cores with logical id 2x+1 are on node 1 */
    /* each node has 16 cores, 32 hyper-threads */
    int phys_id = threadid / 16;
    int core = threadid % 16;

    int logical_id = 2 * core + phys_id;
    /*printf("thread %d bind to logical core %d on physical id %d\n", threadid, logical_id, phys_id);*/

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(logical_id, &set);

    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("Set affinity failed");
        exit(EXIT_FAILURE);
    }
}
#endif

ThreadInfo* threadInfos;
ThreadInfo_qs* threadInfos_qs; 

#ifdef TTAS
Deque globalDeque;
#endif

int a;
unsigned long long Prev1 = 0;
unsigned long long Prev2 = 1;

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int partition(int* array, int low, int high) {
    int pivot = array[high];
    int i = low - 1;

    for (int j = low; j < high; ++j) {
        if (array[j] < pivot) {
            ++i;
            swap(&array[i], &array[j]);
        }
    }

    swap(&array[i + 1], &array[high]);
    return i + 1;
}



void quicksortTask(void* arg, void* arg1) {
    QuicksortTaskArg* qArg = (QuicksortTaskArg*)arg;
    Deque_qs* deque = (Deque_qs*)arg1;
    int* array = qArg->array;
    int low = qArg->low;
    int high = qArg->high;
    if (low < high) {
        int pivot = partition(array, low, high);
        QuicksortTaskArg leftArg = { array, low, pivot - 1 };
        QuicksortTaskArg rightArg = { array, pivot + 1, high };

        // Push tasks for the left and right partitions
        spawnQuicksortTask(&leftArg, deque);
        spawnQuicksortTask(&rightArg, deque);
    }
}



void spawnQuicksortTask(QuicksortTaskArg* qArg, Deque_qs* deque) {
    TaskDescriptor_qsort* task = (TaskDescriptor_qsort*)malloc(sizeof(TaskDescriptor_qsort));
    task->function = quicksortTask;

    // Allocate and set the argument based on the QuicksortTaskArg structure
    task->arg = malloc(sizeof(QuicksortTaskArg));
    *((QuicksortTaskArg*)task->arg) = *qArg;
    task->arg1 = deque;

    #if (TTAS)
    pushBack_qs(deque, task,&sl);
    #elif defined (LCKFREE)
    pushBottom_qs(deque, *task);
    #endif
}


void fibonacciTask(void* arg,void *arg1) {
    int n = *((int*)arg);
    ThreadInfo* info;
    info = (ThreadInfo*)arg1;
    unsigned long long a = Prev1;
    unsigned long long b = Prev2;
    unsigned long long next;

    printf("Thread %d - Fibonacci(%d): ", threadInfos[info->id].id, n);

    for (int i = 0; i < n; ++i) {
        // Mimic Fibonacci computation
        printf("%llu ", a);
        next = a + b;
        a = b;
        b = next;
    }

    Prev1 = a;
    Prev2 = b;

    printf("\n");
}


void spawnFibonacciTask(int n, Deque* deque, ThreadInfo* info) {
    TaskDescriptor* task = (TaskDescriptor*)malloc(sizeof(TaskDescriptor));
    task->function = fibonacciTask;
    task->arg = (int*)malloc(sizeof(int));
    *((int*)task->arg) = n;
    task->arg1 = info;
    #ifdef TTAS
    pushBack(deque, task, &sl);
    #elif defined(LCKFREE)
    pushBottom(deque, *task);
    #endif
}



void executeTasks(ThreadInfo* info) {
    Deque* localDeque = info->localDeque;
    TaskDescriptor* task;

    // First, pop tasks from the local deque
    #ifdef TTAS
    while ((task = popFront(localDeque,&sl)) != NULL) {
        task->function(task->arg,task->arg1);
        free(task->arg);
        free(task);
    }
    #elif defined(LCKFREE)
        while ((task = popTop(localDeque)) != NULL) {
        task->function(task->arg,task->arg1);
        free(task->arg);
        free(task);
    }
    #endif

    // If the local deque is empty, attempt to steal tasks from other threads
    for (int i = 0; i < nthr; ++i) {
        if (i != info->id) {
            Deque* victimDeque = threadInfos[i].localDeque;

            // Try to steal a task from the bottom of the victim's deque
            #if TTAS
            TaskDescriptor* stolenTask = popBack(victimDeque,&sl);
            #elif defined(LCKFREE)
            TaskDescriptor* stolenTask = popBottom(victimDeque);
            #endif
            if (stolenTask != NULL) {
                printf("Thread %d stole a task from Thread %d\n", info->id, threadInfos[i].id);
                stolenTask->function(stolenTask->arg,stolenTask->arg1);
                free(stolenTask->arg);
                //free(stolenTask->arg1);
                free(stolenTask);
                break;  // Steal only one task
            }
        }
    }
}

void executeTasks_qs(ThreadInfo_qs* info) {
    Deque_qs* localDeque = info->localDeque;
    TaskDescriptor_qsort* task;
    TaskDescriptor_qsort* stolenTask;

    // First, pop tasks from the local deque
    #if (TTAS)
    while ((task = popFront_qs(localDeque,&sl)) != NULL) {
        task->function(task->arg,task->arg1);
        free(task->arg);
        free(task);
    }
    #elif defined(LCKFREE)
    while ((task = popTop_qs(localDeque)) != NULL) {
        task->function(task->arg,task->arg1);
        free(task->arg);
        free(task);
    }
    #endif

    // If the local deque is empty, attempt to steal tasks from other threads
    for (int i = 0; i < nthr; ++i) {
        if (i != info->id) {
            Deque_qs* victimDeque = threadInfos_qs[i].localDeque;

            if(victimDeque != NULL)
            {
            // Try to steal a task from the bottom of the victim's deque
            #if (TTAS)
            stolenTask = popBack_qs(victimDeque,&sl);
            #elif defined (LCKFREE)
            stolenTask = popBottom_qs(victimDeque);
            #endif

            }
            if (stolenTask != NULL) {
                printf("Thread %d stole a task from Thread %d\n", info->id, threadInfos_qs[i].id);
                stolenTask->function(stolenTask->arg,stolenTask->arg1);
                //free(stolenTask->arg);
                //free(stolenTask);
                break;  // Steal only one task
            }
        }
    }
}



void initThreadInfo(ThreadInfo* info, int id) {
    info->id = id;
    #if (TTAS)
    info->localDeque = createDeque();
    #elif (LCKFREE)
    info->localDeque = createDeque();
    #endif
    info->fibonacciPrev1 = 0;
    info->fibonacciPrev2 = 1;
}

void initThreadInfo_qs(ThreadInfo_qs* info, int id) {
    info->id = id;
    #if (TTAS)
    info->localDeque = createDeque_qs();
    #elif (LCKFREE)
    info->localDeque = createDeque_qs();
    #endif
}


void* worker(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;

    // Mimic Cilk's randomized work-stealing

    wait_flag(&wflag, nthr);

    if (((long) info->id == 0)) {
        /*printf("get start time\n");*/
        gettimeofday(&start_time, NULL);
    }
    

    int startValue = (info->id) * (TOTAL_OPERATIONS / nthr) + 1;
    int endValue = (info->id + 1) * (TOTAL_OPERATIONS / nthr);

    //printf("start value of %d thread is %d\n",info->id,startValue);
    //printf("End value of %d thread is %d\n",info->id,endValue);




        // Spawn Fibonacci tasks
        for (int j = startValue; j <= endValue; ++j) {
            spawnFibonacciTask(j, info->localDeque, info);
        }

        executeTasks(info);

    if (__sync_fetch_and_add((uint32_t *)&wflag, -1) == 1) {
        gettimeofday(&end_time, NULL);
    }


    return NULL;
}

void* worker_qsort(void* arg) {
    ThreadInfo_qs* info = (ThreadInfo_qs*)arg;

    // Mimic Cilk's randomized work-stealing
    //while (1) {
        // Spawn Quicksort tasks

    wait_flag(&wflag, nthr);

    if (((long) info->id == 0)) {
        /*printf("get start time\n");*/
        gettimeofday(&start_time, NULL);
    }
    
        QuicksortTaskArg qArg = { Qsort_array, 0, ARRAY_SIZE - 1 };
        spawnQuicksortTask(&qArg, info->localDeque);

        executeTasks_qs(info);
    
    if (__sync_fetch_and_add((uint32_t *)&wflag, -1) == 1) {
        gettimeofday(&end_time, NULL);
    }

    
    //}

    return NULL;
}



void printArray(int* array, int size) {
    printf("Sorted Array: ");
    for (int i = 0; i < size; ++i) {
        printf("%d ", array[i]);
    }
    printf("\n");
}







int main(int argc, const char *argv[])
{
    pthread_t *thr;
    int ret = 0;


    #ifdef TTAS
    #endif

    if (argc != 2) {
        printf("Usage: %s <num of threads>\n", argv[0]);
        exit(1);
    }

    Qsort_array = (int*)malloc(ARRAY_SIZE * sizeof(int));
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        Qsort_array[i] = rand() % 100;  // Initialize array with random values
    }

    nthr = atoi(argv[1]);
    //printf("using %d threads\n", nthr);
    thr = calloc(sizeof(*thr), nthr);

    switch (state)
    {
    case Fibonacci:{
        threadInfos = (ThreadInfo*)malloc(nthr * sizeof(ThreadInfo));

    // Start thread
    for (long i = 0; i < nthr; i++) {
        //printf("thread id is %ld", i);
        initThreadInfo(&threadInfos[i], i);
        if (pthread_create(&thr[i], NULL, worker, &threadInfos[i]) != 0) {
            perror("thread creating failed");
        }
    }

    for (long i = 0; i < nthr; i++)
        pthread_join(thr[i], NULL);
    
    free(threadInfos);

    
    printf("Done\n");    
    calc_time(&start_time, &end_time);
        
       //break;
    }
    case QSORT:
    {
    printf("Starting QSORT\n");

    printf("Unsorted Array:\n ");
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        printf("%d ", Qsort_array[i]);
    }
    printf("\n");
    threadInfos_qs = (ThreadInfo_qs*)malloc(nthr * sizeof(ThreadInfo_qs));

    // Start thread
    for (long i = 0; i < nthr; i++) {
        //printf("thread id is %ld", i);
        initThreadInfo_qs(&threadInfos_qs[i], i);
        if (pthread_create(&thr[i], NULL, worker_qsort, &threadInfos_qs[i]) != 0) {
            perror("thread creating failed");
        }
    }

    for (long i = 0; i < nthr; i++)
        pthread_join(thr[i], NULL);
    
    //free(threadInfos_qs);

    
    printf("Done\n");    
    calc_time(&start_time, &end_time);

    printArray(Qsort_array, ARRAY_SIZE);



    

        break;
    }
    
    default:
        break;
    }





    return ret;
}
