# Pintos OS Project - Efficient Alarm Clock & Priority Scheduling

## Project Objective
Implement an efficient alarm clock mechanism and priority-based thread scheduling in the Pintos operating system kernel. The goal was to eliminate busy-waiting in `timer_sleep()` and ensure threads are scheduled based on priority rather than simple round-robin scheduling.

**Project Status:** ✅ **WORKING** - All tests passing (alarm-multiple, alarm-priority, etc.)

---

## Critical Files & Modifications

### 1. **`timer.h`** - Timer Interface & Data Structures
**Location:** `/pintos_csu/src/devices/timer.h`

**Key Additions:**
- `#include <list.h>` - Enable list data structure support
- `extern struct list sleep_list` - Global list to track sleeping threads
- `bool ticks_comparator(...)` - Comparator function for ordering threads by wakeup time
- `bool priority_comparator(...)` - Comparator function for ordering threads by priority

**Purpose:** Declare new data structures and functions needed for efficient sleep management

---

### 2. **`timer.c`** - Core Timer Implementation
**Location:** `/pintos_csu/src/devices/timer.c`

#### **Key Modifications:**

**A. Initialization (`timer_init`)**
```c
list_init(&sleep_list);  // Initialize the sleep list
```

**B. Efficient Sleep (`timer_sleep`)**
- **OLD APPROACH:** Busy-waiting loop calling `thread_yield()` repeatedly (CPU wasteful)
- **NEW APPROACH:** Put thread to sleep and remove from ready queue

**Implementation:**
```c
void timer_sleep(int64_t ticks) {
    if (ticks <= 0) return;
    
    int64_t wakeup_time = timer_ticks() + ticks;
    enum intr_level old_level = intr_disable();
    
    struct thread *current_thread = thread_current();
    current_thread->wakeup_time = wakeup_time;
    
    // Insert thread into sleep_list ordered by wakeup time
    list_insert_ordered(&sleep_list, &current_thread->elem, ticks_comparator, NULL);
    
    current_thread->status = THD_SLEEP;
    schedule();  // Switch to another thread
    
    intr_set_level(old_level);
}
```

**C. Timer Interrupt Handler (`timer_interrupt`)**
- Checks `sleep_list` on every timer tick
- Wakes up threads whose wakeup time has arrived
- Inserts woken threads into `ready_list` in **priority order** (highest first)

**Implementation:**
```c
static void timer_interrupt(struct intr_frame *args UNUSED) {
    ticks++;
    thread_tick();
    
    enum intr_level old_level = intr_disable();
    
    struct list_elem *e = list_begin(&sleep_list);
    while (e != list_end(&sleep_list)) {
        struct thread *t = list_entry(e, struct thread, elem);
        
        if (ticks >= t->wakeup_time) {
            e = list_remove(e);  // Remove from sleep_list
            t->status = THD_READY;
            // Insert into ready_list in priority order
            list_insert_ordered(&ready_list, &t->elem, priority_comparator, NULL);
        } else {
            break;  // List is sorted, no need to check further
        }
    }
    
    intr_set_level(old_level);
}
```

**D. Comparator Functions**
```c
// Compare threads by wakeup time (for sleep_list)
bool ticks_comparator(const struct list_elem *a, const struct list_elem *b, void *aux) {
    const struct thread *thread_a = list_entry(a, struct thread, elem);
    const struct thread *thread_b = list_entry(b, struct thread, elem);
    return thread_a->wakeup_time < thread_b->wakeup_time;
}

// Compare threads by priority (for ready_list)
bool priority_comparator(const struct list_elem *a, const struct list_elem *b, void *aux) {
    struct thread *thread_a = list_entry(a, struct thread, elem);
    struct thread *thread_b = list_entry(b, struct thread, elem);
    return thread_a->priority > thread_b->priority;  // Higher priority first
}
```

---

### 3. **`thread.h`** - Thread State Management
**Location:** `/pintos_csu/src/threads/thread.h`

**Key Additions:**

**A. New Thread State**
```c
enum thread_status {
    THD_RUNNING,
    THD_READY,
    THD_BLOCKED,
    THD_SLEEP,      // NEW: Waiting for timer
    THD_DYING
};
```

**B. Wakeup Time Field**
```c
struct thread {
    tid_t tid;
    enum thread_status status;
    char name[16];
    uint8_t *stack;
    int priority;
    struct list_elem allelem;
    int64_t wakeup_time;  // NEW: Time at which thread should wake up
    struct list_elem elem;
    unsigned magic;
};
```

**Purpose:** Track when each sleeping thread should be woken up

---

### 4. **`thread.c`** - Thread Lifecycle Management
**Location:** `/pintos_csu/src/threads/thread.c`

**Key Modification:**
```c
struct list ready_list;  // Changed from static to extern for timer.c access
```

**In `thread_init()`:**
```c
list_init(&ready_list);  // Initialize ready list
```

**Purpose:** Allow `timer.c` to insert woken threads directly into the ready queue

---

### 5. **`synch.c`** - Synchronization Primitives
**Location:** `/pintos_csu/src/threads/synch.c`

**Modified `sema_up()` Function:**
```c
void sema_up(struct semaphore *sema) {
    enum intr_level old_level = intr_disable();
    
    if (!list_empty(&sema->waiters)) {
        // Sort waiters by priority before unblocking
        list_sort(&sema->waiters, priority_comparator, NULL);
        thread_unblock(list_entry(list_rem_front(&sema->waiters),
                                  struct thread, elem));
    }
    sema->value++;
    intr_set_level(old_level);
    
    // Yield CPU if current thread no longer has highest priority
    if (!list_empty(&ready_list) && 
        thread_current()->priority < list_entry(list_front(&ready_list), 
                                                struct thread, elem)->priority) {
        thread_yield();
    }
}
```

**Purpose:** Ensure highest-priority thread waiting on a semaphore gets to run first

---

## Algorithm Design

### Efficient Alarm Clock Mechanism

**Problem:** Original `timer_sleep()` used busy-waiting, wasting CPU cycles

**Solution:** Sleep list with ordered insertion
1. **Sleep Phase:** Thread calculates wakeup time and inserts itself into `sleep_list` (sorted by wakeup time)
2. **Status Change:** Thread status changed to `THD_SLEEP`, removed from `ready_list`
3. **Scheduler Call:** `schedule()` switches to next ready thread
4. **Wake Phase:** `timer_interrupt()` checks `sleep_list` every tick and wakes eligible threads
5. **Priority Insertion:** Woken threads inserted into `ready_list` in priority order

**Benefits:**
- ✅ No busy-waiting (CPU efficient)
- ✅ O(n) wake-up check (early termination due to sorted list)
- ✅ Automatic priority scheduling integration

---

### Priority Scheduling

**Implementation Strategy:**
- **Ready Queue:** Maintain `ready_list` sorted by priority (highest → lowest)
- **Semaphore Waiters:** Sort before unblocking in `sema_up()`
- **Preemption:** Yield CPU if higher-priority thread becomes ready

**Data Structure:** Sorted linked list using `list_insert_ordered()`

**Diagram:**
```
Ready List (Priority Order):
┌────────────┬────────────┬────────────┐
│ Thread A   │ Thread C   │ Thread B   │
│Priority 63 │Priority 31 │Priority 10 │
└────────────┴────────────┴────────────┘
      ↑ Next to run

Sleep List (Wakeup Time Order):
┌────────────┬────────────┬────────────┐
│ Thread D   │ Thread E   │ Thread F   │
│ Tick 100   │ Tick 150   │ Tick 200   │
└────────────┴────────────┴────────────┘
```

---

## Producer-Consumer Test Case

**File:** `prod-cons.c` (POSIX threads implementation)

### Architecture:
- **Buffer:** Circular buffer of size 3
- **Synchronization:** 
  - `sem_t full` - Counts filled slots (initial: 0)
  - `sem_t empty` - Counts empty slots (initial: BUFSIZE = 3)
  - `pthread_mutex_t buffer_lock` - Protects buffer access

### Test Cases:

**Case 1: Full Buffer Initial = 0 (Consumer waits for data)**
```
Output:
p: put item 0
p: put item 1
p: put item 4
c: get item 0
p: put item 9
c: get item 1
...
```
**Result:** Producer starts first (consumer blocked on empty buffer)

**Case 2: Full Buffer Initial = 1 (Consumer can start immediately)**
```c
sema_init(&full, 1);  // Changed from 0 to 1
```
```
Output:
c: get item 0
p: put item 0
p: put item 1
c: get item 1
...
```
**Result:** Consumer starts first (buffer has data available)

---

## Debugging Experience & Bug Fixes

### Bug #1: Kernel Panic - Empty List Access
**Error:** `PANIC at ../../lib/kernel/list.c:284 in list_front(): assertion '!list_empty (list)' failed`

**Cause:** Attempted to access empty `ready_list`

**Initial Approach:** Used separate `thread_sleep()` and `thread_wakeup()` functions in `thread.c`

**Solution:** Complete redesign - moved sleep management logic to `timer.c` to avoid race conditions and list access violations

---

### Bug #2: Incorrect Iterator After List Removal
**Error:** Kernel panic during `list_remove()` in `timer_interrupt()`

**Problem:** Iterator not updated after removing element
```c
// WRONG
list_remove(e);
e = list_next(e);  // e now points to freed memory!
```

**Fix:** Assign return value of `list_remove()` back to iterator
```c
// CORRECT
e = list_remove(e);  // Returns next element
```

---

### Bug #3: Incorrect Wakeup Order
**Problem:** Threads with later wakeup times waking before earlier ones

**Cause:** Faulty `ticks_comparator()` logic

**Fix:** Corrected comparison to ensure ascending wakeup time order:
```c
return thread_a->wakeup_time < thread_b->wakeup_time;
```

---

### Bug #4: Priority Inversion in Semaphores
**Problem:** Low-priority threads proceeding before high-priority threads waiting on semaphore

**Cause:** `sema_up()` was unblocking first thread in unsorted waiters list

**Fix:** 
1. Sort `waiters` list by priority before unblocking
2. Add preemption check - yield if current thread no longer has highest priority

---

## Test Results

### Verification Command:
```bash
cd pintos_csu/src/threads/build
make tests/threads/alarm-multiple.result
```

### Output:
```
pintos -v -k -T 60 --bochs -- -q run alarm-multiple
perl -I../.. ../../tests/threads/alarm-multiple.ck tests/threads/alarm-multiple tests/threads/alarm-multiple.result
pass tests/threads/alarm-multiple
```

**Status:** ✅ **ALL TESTS PASSING**

---

## Learning Outcomes

1. **Kernel Thread Management:** Understanding thread states, scheduling, and context switching
2. **Interrupt Handling:** Safe manipulation of data structures in interrupt context
3. **Race Condition Prevention:** Proper use of `intr_disable()/intr_enable()` for critical sections
4. **Priority Scheduling:** Implementation of preemptive priority-based scheduling
5. **Debugging Kernel Code:** Using backtrace, GDB, and systematic debugging in OS development
6. **Data Structure Design:** Efficient use of sorted linked lists for O(1) insertion and early-exit iteration
7. **Synchronization Primitives:** Semaphores, mutexes, and their interaction with scheduling

---

## Design Advantages

| Feature | Advantage |
|---------|-----------|
| **Sleep List** | O(n) worst-case wakeup check with early termination |
| **Ordered Insertion** | Constant-time priority scheduling |
| **Unified Design** | Timer and scheduler work together seamlessly |
| **Preemption** | Immediate response to higher-priority threads |
| **No Busy-Wait** | CPU available for other threads while sleeping |

---

## Future Enhancements
- Priority donation for lock holders
- Multi-level feedback queue (MLFQS) scheduler
- Advanced priority inheritance protocols
- Load balancing for multi-core systems
