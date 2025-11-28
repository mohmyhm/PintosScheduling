#include <stdio.h>
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"

#define BUFSIZE 5
#define N 10

static int buffer[BUFSIZE];
static int bufin = 0;
static int bufout = 0;

static struct semaphore empty;
static struct semaphore full;
static struct semaphore start_consumer; // Semaphore to ensure consumer starts first

static struct lock buffer_lock;

static void producer(void *arg UNUSED) {
    int i, item;

    sema_down(&start_consumer); // Wait for consumer to start

    for (i = 0; i < N; i++) {
        sema_down(&empty);
        item = i * i;
        lock_acquire(&buffer_lock);
        buffer[bufin] = item;
        bufin = (bufin + 1) % BUFSIZE;
        lock_release(&buffer_lock);
        printf("p: put item %d\n", item);
        sema_up(&full);
    }
}

static void consumer(void *arg UNUSED) {
    int i, item;

    sema_up(&start_consumer); // Signal producer to start

    for (i = 0; i < N; i++) {
        sema_down(&full);
        lock_acquire(&buffer_lock);
        item = buffer[bufout];
        bufout = (bufout + 1) % BUFSIZE;
        lock_release(&buffer_lock);
        printf("c: get item %d\n", item);
        sema_up(&empty);
    }
}

void test_prod_cons(void) {
    lock_init(&buffer_lock);
    sema_init(&full, 0);
    sema_init(&empty, BUFSIZE);
    sema_init(&start_consumer, 0); // Initialize start_consumer semaphore

    thread_create("Producer", 6, producer, NULL);
    thread_create("Consumer", 60, consumer, NULL);
}

