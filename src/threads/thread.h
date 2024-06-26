#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "lib/kernel/fixpoint.h"
#include "threads/synch.h"
#include "vm/vm.h"

/** States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /**< Running thread. */
    THREAD_READY,       /**< Not running but ready to run. */
    THREAD_BLOCKED,     /**< Waiting for an event to trigger. */
    THREAD_DYING        /**< About to be destroyed. */
  };

/** Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /**< Error value for tid_t. */

/** Thread priorities. */
#define PRI_MIN 0                       /**< Lowest priority. */
#define PRI_DEFAULT 31                  /**< Default priority. */
#define PRI_MAX 63                      /**< Highest priority. */
#define DONATE_MAX 8

#define PRIORITY_UPDATE_FRE 4           /**< Interval ticks when recaculate priority. Used in 4.4BSD*/

#define USR_STACK_MAX (1 << 23)         /**< Max size of user stack. 8mb */

struct supplemental_page_table;
struct donated_priority {
   int priority;
   struct lock *lock;
   struct list_elem elem;
};

struct child_thread {
   tid_t tid;
   int exit_status;                     /**< Exit status of the thread. */
   struct list_elem elem;
   struct semaphore exit_sema;               /**< Semaphore for waiting child thread. */
   bool father_exit;                     /**< If father thread exit, then when child thread exit, it should free child_thread self. */
};

/** A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/** The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /**< Thread identifier. */
    enum thread_status status;          /**< Thread state. */
    struct lock *wait_lock;             /**< Blocking for wait this block*/
    char name[16];                      /**< Name (for debugging purposes). */
    uint8_t *stack;                     /**< Saved stack pointer. */
    int priority;                       /**< Priority. */
    struct list donated_list;           /**< Donated priority list*/
    int origin_priority;                /**< Store origin priority in donated scene*/
    fp recent_cpu;                      /**< CPU time the thread has used recently. Only used with BSD4.4 */
    int nice;                           /**< Nice value for caclulate priority . Only used with BSD4.4 */
    struct list_elem allelem;           /**< List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /**< List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /**< Page directory. */               
    struct list child_list;             /**< List of child threads. */
    struct thread *parent;              /**< Parent thread. */
    struct child_thread *child_self;    /**< Child thread. */
    int next_fd;                      /**< Next file descriptor. */
    struct list file_list;             /**< List of file descriptors. */
    struct file *exec_file;           /**< Executable file. */
   //  size_t usr_stack_size;                   /**< Stack size. */
#endif

#ifdef VM
      struct supplemental_page_table spt; /**< Supplemental page table. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /**< Detects stack overflow. */
  };

/** If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

bool
thread_priority_bigger (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED);


void
thread_try_yiled(void);
void
thread_try_yiled_mlps(void);
void
thread_priority_change (struct thread *t, int new_priority);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_exit_with_status(int status)NO_RETURN;
void thread_yield (void);

/** Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void update_recent_cpu(int64_t ticks);
void update_load_avg(int64_t ticks);
void update_priority(int64_t ticks);
#endif /**< threads/thread.h */
