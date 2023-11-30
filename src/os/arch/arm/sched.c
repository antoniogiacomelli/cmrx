/** @defgroup os_sched Kernel scheduler
 *
 * @ingroup os_kernel
 *
 * @brief Kernel scheduler internals
 *
 */

/** @ingroup os_sched
 * @{
 */
#include <cmrx/os/runtime.h>
#include <cmrx/os/sched.h>
#include <cmrx/os/timer.h>
#include <cmrx/os/arch/sched.h>
#include <arch/memory.h>
#include <arch/cortex.h>
#include <cmrx/os/mpu.h>
#include <arch/mpu.h>
#include <arch/mpu_priv.h>
#include <string.h>

#ifdef TESTING
#define STATIC
#else
#define STATIC static
#endif

#include <cmrx/assert.h>

/*
int __os_process_create(Process_t process_id, const struct OS_process_definition_t * definition);
int __os_thread_create(Process_t process, entrypoint_t * entrypoint, void * data, uint8_t priority);
int os_thread_alloc(Process_t process, uint8_t priority);
void os_thread_dispose(int arg0);
__attribute__((noreturn)) int os_idle_thread(void * data);
bool os_get_next_thread(uint8_t current_thread, uint8_t * next_thread);
int os_stack_create();
unsigned long * os_stack_get(int stack_id);
struct OS_thread_t * os_thread_get(Thread_t thread_id);
void os_sched_timed_event(void);
*/

/** Populate stack of new thread so it can be executed.
 * Populates stack of new thread so that it can be executed with no
 * other actions required. Returns the address where SP shall point to.
 * @param stack_id ID of stack to be populated
 * @param stack_size size of stack in 32-bit quantities
 * @param entrypoint address of thread entrypoint function
 * @param data address of data passed to the thread as its 1st argument
 * @returns Address to which the SP shall be set.
 */
uint32_t * os_thread_populate_stack(int stack_id, unsigned stack_size, entrypoint_t * entrypoint, void * data)
{
    unsigned long * stack = os_stack_get(stack_id);
    stack[stack_size - 8] = (unsigned long) data; // R0
    stack[stack_size - 3] = (unsigned long) os_thread_dispose; // LR
    stack[stack_size - 2] = (unsigned long) entrypoint; // PC
    stack[stack_size - 1] = 0x01000000; // xPSR

    return &stack[stack_size - 16];

}


/** Create process using process definition.
 * Takes process definition and initializes MPU regions for process out of it.
 * @param process_id ID of process to be initialized
 * @param definition process definition. This is constructed at compile time using OS_APPLICATION macros
 * @returns E_OK if process was contructed properly, E_INVALID if process ID is already used or
 * if process definition contains invalid section boundaries. E_OUT_OF_RANGE is returned if process ID
 * requested is out of limits given by the size of process table.
 */
int os_process_create(Process_t process_id, const struct OS_process_definition_t * definition)
{
	if (process_id >= OS_PROCESSES)
	{
		return E_OUT_OF_RANGE;
	}
	
	if (os_processes[process_id].definition != NULL)
	{
		return E_INVALID;
	}

	os_processes[process_id].definition = definition;
	for (int q = 0; q < OS_TASK_MPU_REGIONS; ++q)
	{
		unsigned reg_size = (uint8_t *) definition->mpu_regions[q].end - (uint8_t *) definition->mpu_regions[q].start;
		int rv = mpu_configure_region(q, definition->mpu_regions[q].start, reg_size, MPU_RW, &os_processes[process_id].mpu[q]._MPU_RBAR, &os_processes[process_id].mpu[q]._MPU_RASR);
		if (rv != E_OK)
		{
			os_processes[process_id].definition = NULL;
			return E_INVALID;
		}
	}
	return E_OK;
}

__attribute__((naked,noreturn)) void os_boot_thread(Thread_t boot_thread)
{
    // Start this thread
    // We are adding 8 here, because normally pend_sv_handler would be reading 8 general 
    // purpose registers here. But there is nothing useful there, so we simply skip it.
    // Code belog then restores what would normally be restored by return from handler.
    struct OS_thread_t * thread = os_thread_get(boot_thread);
    unsigned long * thread_sp = thread->sp + 8;
    __set_PSP((uint32_t) thread_sp);
    __set_CONTROL(0x03); 	// SPSEL = 1 | nPRIV = 1: use PSP and unpriveldged thread mode

    __ISB();

    __ISR_return();

}

/** @} */