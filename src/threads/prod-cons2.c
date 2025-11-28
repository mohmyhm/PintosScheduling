#include <stdio.h>
#include <string.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"


#define N 10
#define BUFSIZE 3


static int buffer[BUFSIZE];
static int bufin = 0, bufout = 0;

static struct semaphore empty, full;
static struct lock buffer_lock;
static void *producer(void *arg1) {
   int i, item;

   for (i = 0; i < N; i++) {
      sem_wait(&empty);
      item = i*i;
      pthread_mutex_lock(&buffer_lock);
      buffer[bufin] = item;    
      bufin = (bufin + 1) % BUFSIZE;
      pthread_mutex_unlock(&buffer_lock);
      printf("p: put item %d\n", item); 
      sem_post(&full);
   }   
   return NULL;
}

static void *consumer(void *arg2) {
   int i, item;

   for (i = 0; i < N; i++) {
      sem_wait(&full);
      pthread_mutex_lock(&buffer_lock);
      item = buffer[bufout];
      bufout = (bufout + 1) % BUFSIZE;
      pthread_mutex_unlock(&buffer_lock);
      printf("c: get item %d\n",item);
      sem_post(&empty);
   }   
   return NULL;
}
 
void main(void) {
   pthread_t prodtid, constid;
   sem_init(&full, 0, 0);
   sem_init(&empty, 0, BUFSIZE);
   pthread_create(&prodtid, NULL, producer, NULL);
   pthread_create(&constid, NULL, consumer, NULL);
   pthread_exit(0);
}

