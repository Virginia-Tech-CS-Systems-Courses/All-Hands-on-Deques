#ifndef _DEQ_Lock_H
#define _DEQ_Lock_H

#include "spinlock-TTAS.h"
#define DEQUE_CAPACITY 100

/*QSORT*/
typedef struct TaskDescriptor_qsort {
    void (*function)(void*);  // Function pointer for task execution
    void* arg;                 // Argument for the task function
} TaskDescriptor_qsort;

typedef struct QuicksortTaskArg {
    int* array;
    int low;
    int high;
} QuicksortTaskArg; 

typedef struct Deque_qs {
    TaskDescriptor_qsort* tasks[DEQUE_CAPACITY];
    int front, rear;
} Deque_qs



void initDeque_qs(Deque_qs* deque) {
    deque->front = -1;
    deque->rear = -1;
}

Deque_qs* createDeque() {
    Deque_qs* deque = (Deque_qs*)malloc(sizeof(Deque_qs));
    if (deque != NULL) {
        initDeque(deque);
    }
    return deque;
}


void initDeque(Deque_qs* deque) {
    deque->front = -1;
    deque->rear = -1;
}

int isDequeEmpty_qs(Deque_qs* deque) {
    return deque->front == -1;
}

void pushFront_qs(Deque_qs* deque, TaskDescriptor_qsort* task, spinlock *lock) {
    spin_lock_ttas(lock);
    if (isDequeEmpty(deque)) {
        deque->front = 0;
        deque->rear = 0;
    } else {
        deque->front = (deque->front - 1 + DEQUE_CAPACITY) % DEQUE_CAPACITY;
    }
    deque->tasks[deque->front] = task;
    spin_unlock_ttas(lock);
}

TaskDescriptor_qsort* popFront_qs(Deque_qs* deque, spinlock *lock) {
    spin_lock_ttas(lock);
    if (isDequeEmpty(deque)) {
        spin_unlock_ttas(lock);
        return NULL;
    }
    TaskDescriptor_qsort* task = deque->tasks[deque->front];
    if (deque->front == deque->rear) {
        deque->front = -1;
        deque->rear = -1;
    } else {
        deque->front = (deque->front + 1) % DEQUE_CAPACITY;
    }
    spin_unlock_ttas(lock);
    return task;
}

void pushBack_qs(Deque_qs* deque, TaskDescriptor_qsort* task, spinlock *lock) {
    spin_lock_ttas(lock);
    if (isDequeEmpty(deque)) {
        deque->front = 0;
        deque->rear = 0;
    } else {
        deque->rear = (deque->rear + 1) % DEQUE_CAPACITY;
    }
    deque->tasks[deque->rear] = task;
    spin_unlock_ttas(lock);
}

TaskDescriptor_qsort* popBack_qs(Deque_qs* deque, spinlock *lock) {
    spin_lock_ttas(lock);
    if (isDequeEmpty(deque)) {
        spin_unlock_ttas(lock);
        return NULL;
    }
    TaskDescriptor_qsort* task = deque->tasks[deque->rear];
    if (deque->front == deque->rear) {
        deque->front = -1;
        deque->rear = -1;
    } else {
        deque->rear = (deque->rear - 1 + DEQUE_CAPACITY) % DEQUE_CAPACITY;
    }
    spin_unlock_ttas(lock);
    return task;
}


#endif /* _DEQ_Lock_H */
