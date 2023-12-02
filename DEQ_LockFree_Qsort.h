#ifndef P3_LOCKFREE_qs_H
#define P3_LOCKFREE_qs_H

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct TaskDescriptor_qsort {
    void (*function)(void*, void*);
    void* arg;
    void* arg1;
} TaskDescriptor_qsort;

typedef struct Node_qs {
    TaskDescriptor_qsort task;
    struct Node_qs* next;
} Node_qs;

typedef struct {
    Node_qs* ptr;
    unsigned long tag;
} TaggedPtr_qs;

typedef struct {
    _Atomic(TaggedPtr_qs) head;
    _Atomic(TaggedPtr_qs) tail;
} Deque_qs;

Node_qs* newNode_qs(TaskDescriptor_qsort task) {
    Node_qs* node = (Node_qs*)malloc(sizeof(Node_qs));
    node->task = task;
    node->next = NULL;
    return node;
}

void initDeque_qs(Deque_qs* deque) {
    TaskDescriptor_qsort dummyTask = {NULL, NULL, NULL};
    Node_qs* dummy = newNode_qs(dummyTask); // Dummy node
    TaggedPtr_qs dummyTagged = {dummy, 0};
    atomic_store(&deque->head, dummyTagged);
    atomic_store(&deque->tail, dummyTagged);
}

int isDequeEmpty_qs(Deque_qs* deque) {
    TaggedPtr_qs head = atomic_load_explicit(&deque->head, memory_order_acquire);
    TaggedPtr_qs tail = atomic_load_explicit(&deque->tail, memory_order_acquire);
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
void pushBottom_qs(Deque_qs* deque, TaskDescriptor_qsort task) {
    Node_qs* new_node = newNode_qs(task);
    TaggedPtr_qs tail, new_tail, head;

    do {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);
        tail = atomic_load_explicit(&deque->tail, memory_order_acquire);

        if (head.ptr == NULL) { // Check if the deque is empty
            // Initialize the head if deque is empty
            TaggedPtr_qs new_head = {new_node, 0};
            if (atomic_compare_exchange_weak_explicit(&deque->head, &head, new_head, memory_order_release, memory_order_relaxed)) {
                atomic_store_explicit(&deque->tail, new_head, memory_order_release);
                return;
            }
        }

        new_tail.ptr = new_node;
        new_tail.tag = tail.tag + 1;
    } while (!atomic_compare_exchange_weak_explicit(&deque->tail, &tail, new_tail, memory_order_release, memory_order_relaxed));

    Node_qs* expected = NULL;
    atomic_compare_exchange_strong_explicit(&tail.ptr->next, &expected, new_node, memory_order_release, memory_order_relaxed);
}


TaskDescriptor_qsort* popBottom_qs(Deque_qs* deque) {
    if (isDequeEmpty_qs(deque)) {
        return NULL;
    }
    TaggedPtr_qs tail, prev_tail, head;
    Node_qs* current;

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
            TaskDescriptor_qsort* data = malloc(sizeof(TaskDescriptor_qsort));
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

void pushTop_qs(Deque_qs* deque, TaskDescriptor_qsort task) {
    Node_qs* new_node = newNode_qs(task);
    TaggedPtr_qs head, new_head;

    do {
        head = atomic_load_explicit(&deque->head, memory_order_acquire);

        if (head.ptr == NULL) { // Check if the deque is empty
            // Initialize the head if deque is empty
            TaggedPtr_qs new_head = {new_node, 0};
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


TaskDescriptor_qsort* popTop_qs(Deque_qs* deque) {
    if (isDequeEmpty_qs(deque)) {
        return NULL;
    }
    TaggedPtr_qs head, next, tail, firstRealNode;
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

    TaskDescriptor_qsort* data = malloc(sizeof(TaskDescriptor_qsort));
    if (data != NULL) {
        *data = firstRealNode.ptr->task;
    }
    free(firstRealNode.ptr);
    return data;
}

Deque_qs* createDeque_qs() {
    Deque_qs* deque_qs = (Deque_qs*)malloc(sizeof(Deque_qs));
    if (deque_qs != NULL) {
        initDeque_qs(deque_qs);
    }
    return deque_qs;
}




#endif //P3_LOCKFREE_qs_H
