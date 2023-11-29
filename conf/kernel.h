/** @ingroup config
 * @{
 */
#pragma once

/** This turns on memory protection globally */
#define KERNEL_HAS_MEMORY_PROTECTION
//#undef KERNEL_HAS_MEMORY_PROTECTION

/** How many MPU regions are saved per thread */
#define MPU_STATE_SIZE			7

/** How many MPU regions are always used based on in which process thread is hosted */
#define MPU_HOSTED_STATE_SIZE	4

/** How many MPU regions can process define */
#define OS_TASK_MPU_REGIONS		5

/** How big stack is? In bytes */
#define OS_STACK_SIZE			1024

/** How many threads can exist */
#define OS_THREADS				8

/** How many stacks can be allocated */
#define OS_STACKS				8

/** How many processes can be allocated */
#define OS_PROCESSES 			8

/** How many sleeping threads can exist */
#define SLEEPERS_MAX			(2 * OS_THREADS)

/** @} */
