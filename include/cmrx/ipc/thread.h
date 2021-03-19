#pragma once

#include <cmrx/os/sysenter.h>

/** Return current thread ID.
 *
 * @returns thread ID of currently running thread.
 */
__SYSCALL int get_tid();

/** Give up processor.
 *
 * Call to this method cause thread switch. If thread switch occurs, or not, depends
 * on how thread priorities are configured. If there is no other thread ready at
 * equal or higher priority than currently running thread, then switch won't occurr.
 * @returns 0. Mostly.
 */
__SYSCALL int sched_yield();

/** Create new thread.
 *
 * Creates new thread and prepares it for scheduling. This routine can be used for 
 * on-demand thread creation. New thread will be bound to current process (the one
 * which owns currently running thread). You can set up thread entrypoint, pass it
 * some user data and opt for thread priority.
 * @param entrypoint function, which will be called upon thread startup to run the
 * thread
 * @param data user-defined data passed to the entrypoint as first argument
 * @priority priority of newly created thread. Lower numbers mean higher priorities.
 * Thread with priority 0 has realtime priority, thread with priority 255 is an idle
 * thread. Note that there already is one idle thread and if you create another, 
 * then outcome most probably won't be as expected. Use priority 254 for custom
 * idle threads.
 * @returns non-negative numbers carrying thread ID of newly created thread or 
 * negative numbers to signal error.
 */
__SYSCALL int thread_create(int *entrypoint(void *), void * data, uint8_t priority);

/** Wait for other thread to finish.
 * 
 * This function will block calling thread until other thread quits.
 * @param thread thread ID of other threads, which this thread wants to fair for
 * @param status place for return value from other thread to be written
 * @returns 0 on success (other thread quit and status value is written), error
 * code otherwise.
 */
__SYSCALL int thread_join(int thread, int * status);

/** Terminate currently running thread.
 *
 * This function will explicitly terminate currently running thread.
 * Another way to implicitly terminate running thread is to return from thread
 * entry function. Thread will be terminated automatically using return value as
 * thread exit status in a way similar to how return from main() is used as process
 * exit status.
 * @param status thread exit status
 * @return You don't want this function to return.
 */
__SYSCALL int thread_exit(int status);

