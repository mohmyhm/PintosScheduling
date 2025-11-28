static struct semaphore start_consumer;

void test_prod_cons(void) {
   lock_init(&buffer_lock);
   sema_init(&full, 1);
   sema_init(&empty, BUFSIZE);
   sema_init(&start_consumer, 0); // Initialize the start_consumer semaphore to 0
   thread_create("Producer",4, producer, NULL);
   thread_create("Consumer",6 , consumer, NULL);
   sema_up(&start_consumer); // Allow the consumer to start
}

static void consumer(void *arg UNUSED) {
   int i, item;
   sema_down(&start_consumer); // Wait for the signal to start

   for (i = 0; i < N; i++) {
      sema_down(&full);
      lock_acquire(&buffer_lock);
      item = buffer[bufout];
      bufout = (bufout + 1) % BUFSIZE;
      lock_release(&buffer_lock);
      printf("c: get item %d\n",item);
      sema_up(&empty);
   }
}
