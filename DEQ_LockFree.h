#ifndef P3_LOCKFREE_H
#define P3_LOCKFREE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct TaskDescriptor {
    void (*function)(void*, void*);
    void* arg;
    void* arg1;
} TaskDescriptor;

typedef struct Node {
    TaskDescriptor task;
    struct Node* next;
} Node;

typedef struct {
    Node* ptr;
    unsigned long tag;
} TaggedPtr;

typedef struct {
    _Atomic(TaggedPtr) head;
    _Atomic(TaggedPtr) tail;
} Deque;

Node* newNode(TaskDescriptor task) {
    Node* node = (Node*)malloc(sizeof(Node));
    node->task = task;
    node->next = NULL;
    return node;
}

void initDeque(Deque* deque) {
    TaskDescriptor dummyTask = {NULL, NULL, NULL};
    Node* dummy = newNode(dummyTask); // Dummy node
    TaggedPtr dummyTagged = {dummy, 0};
    atomic_store(&deque->head, dummyTagged);
    atomic_store(&deque->tail, dummyTagged);
}

int isDequeEmpty(Deque* deque) {
    TaggedPtr head = atomic_load_explicit(&deque->head, memory_order_acquire);
    TaggedPtr tail = atomic_load_explicit(&deque->tail, memory_order_acquire);
    return head.ptr == tail.ptr || head.ptr->next == NULL;
}

/*
void pushBottom(Deque* deque, TaskDescriptor task) {
    Node* new_node = newNode(task);
    TaggedPtr tail, new_tail;
    do {
        tail = atomic_load_explicit(&deque->tail, memory_order_acquire);
        new_tail.ptr = new_node;
        new_tail.tag = tail.tag + 1;
    } while (!atomic_compare_exchange_weak_explicit(&deque->tail, &tail, new_tail, memory_order_release, memory_order_relaxed));

    Node* expected = NULL;
    atomic_compare_exchange_strong_explicit(&tail.ptr->next, &expected, new_node, memory_order_release, memory_order_relaxed);
}*/
void pushBottom(Deque* deque, TaskDescriptor task) {
    Node* new_node = newNode(task);
    TaggedPtr tail, new_tail, head;

    do {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);
        tail = atomic_load_explicit(&deque->tail, memory_order_acquire);

        if (head.ptr == NULL) { // Check if the deque is empty
            // Initialize the head if deque is empty
            TaggedPtr new_head = {new_node, 0};
            if (atomic_compare_exchange_weak_explicit(&deque->head, &head, new_head, memory_order_release, memory_order_relaxed)) {
                atomic_store_explicit(&deque->tail, new_head, memory_order_release);
                return;
            }
        }

        new_tail.ptr = new_node;
        new_tail.tag = tail.tag + 1;
    } while (!atomic_compare_exchange_weak_explicit(&deque->tail, &tail, new_tail, memory_order_release, memory_order_relaxed));

    Node* expected = NULL;
    atomic_compare_exchange_strong_explicit(&tail.ptr->next, &expected, new_node, memory_order_release, memory_order_relaxed);
}


TaskDescriptor* popBottom(Deque* deque) {
    if (isDequeEmpty(deque)) {
        return NULL;
    }
    TaggedPtr tail, prev_tail, head;
    Node* current;

    while (true) {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);
        tail = atomic_load_explicit(&deque->tail, memory_order_acquire);

        if (head.ptr == tail.ptr || tail.ptr == NULL) {  // Check if deque is empty or tail is null
            return NULL;
        }

        // Traverse the deque to find the node before tail
        current = head.ptr;
        while (current->next != tail.ptr && current->next != NULL) {
            current = current->next;
        }

        // If reached the end of the list or list is corrupted, retry
        if (current->next == NULL) {
            continue;
        }

        prev_tail.ptr = current;
        prev_tail.tag = tail.tag + 1;

        // Attempt to update the tail
        if (atomic_compare_exchange_weak_explicit(&deque->tail, &tail, prev_tail, memory_order_release, memory_order_relaxed)) {
            if (tail.ptr == NULL) {  // Additional check for null tail
                return NULL;
            }
            TaskDescriptor* data = malloc(sizeof(TaskDescriptor));
            if (data) {
                *data = tail.ptr->task;
            }
            free(tail.ptr);  // Free the node
            return data;
        }
    }
}

/*
void pushTop(Deque* deque, TaskDescriptor task) {
    Node* new_node = newNode(task);
    TaggedPtr head, new_head;
    do {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);
        new_node->next = head.ptr->next;
        new_head.ptr = new_node;
        new_head.tag = head.tag + 1;
    } while (!atomic_compare_exchange_weak_explicit(&head.ptr->next, &head.ptr->next, new_node, memory_order_release, memory_order_relaxed));
}
*/

void pushTop(Deque* deque, TaskDescriptor task) {
    Node* new_node = newNode(task);
    TaggedPtr head, new_head;

    do {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);

        if (head.ptr == NULL) { // Check if the deque is empty
            // Initialize the head if deque is empty
            TaggedPtr new_head = {new_node, 0};
            if (atomic_compare_exchange_weak_explicit(&deque->head, &head, new_head, memory_order_release, memory_order_relaxed)) {
                atomic_store_explicit(&deque->tail, new_head, memory_order_release);
                return;
            }
        }

        new_node->next = head.ptr;
        new_head.ptr = new_node;
        new_head.tag = head.tag + 1;
    } while (!atomic_compare_exchange_weak_explicit(&deque->head, &head, new_head, memory_order_release, memory_order_relaxed));
}


TaskDescriptor* popTop(Deque* deque) {
    if (isDequeEmpty(deque)) {
        return NULL;
    }
    TaggedPtr head, next, tail, firstRealNode;
    do {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);
        tail = atomic_load_explicit(&deque->tail, memory_order_acquire);

        firstRealNode.ptr = head.ptr->next;
        if (firstRealNode.ptr == NULL || firstRealNode.ptr == tail.ptr) {
            return NULL; // Deque is empty
        }

        next.ptr = firstRealNode.ptr->next;
        next.tag = head.tag + 1;

    } while (!atomic_compare_exchange_weak_explicit(&head.ptr->next, &firstRealNode.ptr, next.ptr, memory_order_release, memory_order_relaxed));

    TaskDescriptor* data = malloc(sizeof(TaskDescriptor));
    if (data != NULL) {
        *data = firstRealNode.ptr->task;
    }
    free(firstRealNode.ptr);
    return data;
}

Deque* createDeque() {
    Deque* deque = (Deque*)malloc(sizeof(Deque));
    if (deque != NULL) {
        initDeque(deque);
    }
    return deque;
}




#endif //P3_LOCKFREE_H
