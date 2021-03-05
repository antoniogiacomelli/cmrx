#include <cmrx/ipc/mutex.h>
#include <cmrx/intrinsics.h>
#include <cmrx/defines.h>
#include <cmrx/sched.h>

/** Lock futex.
 * Perform atomic futex lock. It is possible to lock futex which is either completely unlocked,
 * or a recursive futex, which has still some space left for locking. During locking, it is
 * checked if futex owner matches. If futex lock level is too deep or futex is owned by someone
 * else, then futex lock fails.
 * @param futex futex to be locked
 * @param thread_id identification of calling thread
 * @param max_depth maximum depth futex can already be locked in order to be still able to lock it
 * @returns 0 if futex lock was successful, 1 if locking failed for whatever reason
 */
static inline int __futex_fast_lock(futex_t * futex, uint8_t thread_id, unsigned max_depth)
{
	int success;
	register unsigned mutexOwner = 0xFE,  mutexState = 0xFF;
	asm volatile(
			// success == 1, for bailouts it means that STREX failed to store value
			"MOV %[mutexSuccess], #1\n\t"

			// load mutex->state and mutex->owner values, state is loaded exclusively
			"LDREXB %[mutexState], [%[mutexStateAddr]]\n\t"
			"LDRB %[mutexOwner], [%[mutexOwnerAddr]]\n\t"

			// is mutex claimed? 
			"CMP %[mutexOwner], #0xFF\n\t"
			"BEQ .futex_fast_lock_not_owned\n\t"
			// mutex is claimed by someone, by us?
			"CMP %[mutexOwner], %[threadId]\n\t"
			"BNE .futex_fast_lock_non_lockable\n"

			// mutex is either not owned, or owned by us
			// compare if mutex is suitable for locking
			".futex_fast_lock_not_owned:"
			"CMP %[mutexState], %[maxDepth]\n\t"
			"BGT .futex_fast_lock_non_lockable\n\t"

			// mutex is lockable, so lock it
			"ADD %[mutexState], %[mutexState], #1\n\t"
			"STREXB %[mutexSuccess], %[mutexState], [%[mutexStateAddr]]\n"
			".futex_fast_lock_non_lockable:\n\t"
			// clear exclusivity and return
			"CLREX\n\t"
			: [mutexSuccess] "=&r" (success),
			  [mutexOwner] "+r" (mutexOwner),
			  [mutexState] "+r" (mutexState)
			: [mutexStateAddr] "r" (&futex->state),
			  [mutexOwnerAddr] "r" (&futex->owner),
			  [threadId] "r" (thread_id),
			  [maxDepth] "r" (max_depth)
			);
	return success;
}

/** Unlock futex.
 * This function performs fast unlock of futex if that is possible.
 * It first checks, if futex is locked by current thread and if
 * it is actually locked. If these conditions are met, then
 * futex unlock is performed.
 * @param futex Futex to be unlocked
 * @param thread_id Numeric identification of futex owner
 * @returns 0 if futex unlock was successful, 1 if unlocking failed for
 * whatever reason.
 */
static inline int __futex_fast_unlock(futex_t * futex, uint8_t thread_id)
{
	int success;
	register unsigned mutexOwner, mutexState;
	asm volatile(
			".syntax unified\n\t"
			// success == 1, for bailouts it means that STREX failed to store value
			"MOV %[mutexSuccess], #1\n\t"

			// load mutex->state and mutex->owner values, state is loaded exclusively
			"LDREXB %[mutexState], [%[mutexStateAddr]]\n\t"
			"LDRB %[mutexOwner], [%[mutexOwnerAddr]]\n\t"

			// check if mutex is suitable for locking (must be non-zero)
			"CBZ %[mutexState], .futex_fast_unlock_not_unlockable\n\t"

			// check if mutex is claimed by us currently
			"CMP %[mutexOwner], %[threadId]\n\t"
			"BNE .futex_fast_unlock_not_unlockable\n\t"

			// mutex is unlockable, so unlock it
			"SUB %[mutexState], %[mutexState], #1\n\t"
			"STREXB %[mutexSuccess], %[mutexState], [%[mutexStateAddr]]\n"
			".futex_fast_unlock_not_unlockable:\n\t"
			// clear exclusivity and return
			"CLREX\n\t"
			: [mutexSuccess] "=&r" (success),
			  [mutexOwner] "=&r" (mutexOwner),
			  [mutexState] "=&r" (mutexState)
			: [mutexStateAddr] "r" (&futex->state),
			  [mutexOwnerAddr] "r" (&futex->owner),
			  [threadId] "r" (thread_id)
			);
	return success;
}

int mutex_init(mutex_t * restrict mutex)
{
	mutex->owner = 0xFF;
	mutex->flags |= MUTEX_INITIALIZED;
	mutex->state = 0;
	return 0;
}

int mutex_destroy(mutex_t * mutex)
{
	mutex->owner = 0xFF;
	mutex->flags = 0;
	mutex->state = 0;
	return 0;
}

int mutex_lock(mutex_t * mutex)
{
	// claim "exclusivity" on mutex
	uint8_t tmp_state;
	uint8_t thread_id = get_tid();
	while (1) {
		tmp_state = __LDREXB(&mutex->state);
		if (mutex->owner != thread_id && mutex->owner != 0xFF)
		{
			sched_yield();
			continue;
		}
		if (tmp_state == 0)
		{
			int success = __STREXB(&mutex->state, 1);
			// STREXB writes 0, if store was successful
			if (success == 0)
			{
				mutex->owner = thread_id;
				return 0;
			}
			else
			{
				sched_yield();
				continue;
			}
		}
	}
}


int mutex_trylock(mutex_t * mutex)
{
	// claim "exclusivity" on mutex
	uint8_t tmp_state = __LDREXB(&(mutex->state));
	uint8_t thread_id = get_tid();
	if (mutex->owner != thread_id)
	{
		__CLREX();
		return E_BUSY;
	}
	if (tmp_state == 0)
	{
		int success = __STREXB(&(mutex->state), 1);
		if (success == 0)
		{
			mutex->owner = thread_id;
			return 0;
		}
		else
		{
			return E_DEADLK;
		}
	}
	__CLREX();
	return E_DEADLK;
}

int mutex_unlock(mutex_t * mutex)
{
	uint8_t tmp_state = __LDREXB(&mutex->state);
	uint8_t thread_id = get_tid();
	if (mutex->owner != thread_id)
	{
		__CLREX();
		return E_BUSY;
	}

	if (tmp_state == 0)
	{
		// mutex is unlocked, so what now?
		__CLREX();
		return 0;
	}

	int success = 1;
	do
	{
		tmp_state = __LDREXB(&mutex->state);
		success = __STREXB(&mutex->state, 0);
	} while (success != 0);
	
	mutex->owner = 0xFF;

	return 0;
}

int futex_init(futex_t * restrict futex)
{
	futex->owner = 0xFF;
	futex->state = 0;
	futex->flags = 0;
	return 0;
}

int futex_lock(futex_t * futex)
{
	uint8_t thread_id = get_tid();
	int success;
	do {
		success = __futex_fast_lock(futex, thread_id, 0);
		if (success != 0)
		{
			sched_yield();
		}
	} while (success != 0);
	futex->owner = thread_id;
	return 0;
}

int futex_trylock(futex_t * futex)
{
	uint8_t thread_id = get_tid();
	int success = __futex_fast_lock(futex, thread_id, 0);
	return success;
}

int futex_unlock(futex_t * futex)
{
	uint8_t thread_id = get_tid();
	int success = __futex_fast_unlock(futex, thread_id);
	if (success == 0 && futex->state == 0)
	{
		futex->owner = 0xFF;
	}
	return success;
}