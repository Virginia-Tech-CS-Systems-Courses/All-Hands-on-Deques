#ifndef _DEQ_Lock_H
#define _DEQ_Lock_H

#include "spinlock-TTAS.h"
#define DEQUE_CAPACITY 100

typedef struct TaskDescriptor {
    void (*function)(void*,void*);  // Function pointer for task execution
    void* arg;                 // Argument for the task function
    void* arg1;
} TaskDescriptor;

typedef struct Deque {
    TaskDescriptor* tasks[DEQUE_CAPACITY];
    int front, rear;
} Deque;

void initDeque(Deque* deque) {
    deque->front = -1;
    deque->rear = -1;
}

int isDequeEmpty(Deque* deque) {
    return deque->front == -1;
}

void pushFront(Deque* deque, TaskDescriptor* task, spinlock *lock) {
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

TaskDescriptor* popFront(Deque* deque, spinlock *lock) {
    spin_lock_ttas(lock);
    if (isDequeEmpty(deque)) {
        spin_unlock_ttas(lock);
        return NULL;
    }
    TaskDescriptor* task = deque->tasks[deque->front];
    if (deque->front == deque->rear) {
        deque->front = -1;
        deque->rear = -1;
    } else {
        deque->front = (deque->front + 1) % DEQUE_CAPACITY;
    }
    spin_unlock_ttas(lock);
    return task;
}

void pushBack(Deque* deque, TaskDescriptor* task, spinlock *lock) {
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

TaskDescriptor* popBack(Deque* deque, spinlock *lock) {
    spin_lock_ttas(lock);
    if (isDequeEmpty(deque)) {
        spin_unlock_ttas(lock);
        return NULL;
    }
    TaskDescriptor* task = deque->tasks[deque->rear];
    if (deque->front == deque->rear) {
        deque->front = -1;
        deque->rear = -1;
    } else {
        deque->rear = (deque->rear - 1 + DEQUE_CAPACITY) % DEQUE_CAPACITY;
    }
    spin_unlock_ttas(lock);
    return task;
}

Deque* createDeque() {
    Deque* deque = (Deque*)malloc(sizeof(Deque));
    if (deque != NULL) {
        initDeque(deque);
    }
    return deque;
}

#endif /* _DEQ_Lock_H */
