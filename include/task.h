
#pragma once

#include <types.h>
#include <list.h>
#include <rbtree.h>
#include <spinlock.h>

#include <x86-64/idt.h>
#include <x86-64/memory.h>

typedef int32_t pid_t;

// Bit i set means the task may run on CPU i
typedef uint64_t cpu_set_t;

/* Values of task_status in struct task. */
enum {
	TASK_DYING = 0,
	TASK_RUNNABLE,
	TASK_RUNNING,
	TASK_NOT_RUNNABLE,
	TASK_SLEEPING,
};

/* The method of interrupt used to switch to the kernel. */
enum {
	TASK_INT = 0,
	TASK_SYSCALL,
};

/* Special task types. */
enum task_type {
	TASK_TYPE_USER = 0,
	TASK_TYPE_KERNEL,
};

struct task {
	/* The saved registers. */
	struct int_frame task_frame;

	/* The task this task is waiting on. */
	struct task *task_wait;
	
	/* The process ID of this task and its parent. */
	pid_t task_pid;
	pid_t task_ppid;

	/* The task type. */
	enum task_type task_type;

	/* The task status. */
	unsigned task_status;

	/* Killed by its parent, which waits for its core to drop it. */
	bool task_killed;

	/* The number of times the task has been run. */
	unsigned task_runs;

	/* CPU time (in TSC ticks) the task has used, as seen by the scheduler. */
	uint64_t task_karma;

	/* TSC value when the task most recently started running. */
	uint64_t task_start_tsc;

	/* The TSC value when the task should wake up. */
	uint64_t task_wakeup_tsc;

	/* The node in the run queue (ordered by karma) when the task is runnable
	 * The node in the sleep queue (ordered by wakeup time) when it sleeps */
	struct rb_node task_sched_rb;

	/* The exit status of the task in case it has died */
	int task_exit_status;

	/* The CPU that the task is running on. */
	int task_cpunum;

	/* The CPUs the task is allowed to run on. */
	uint64_t task_affinity;

	/* The virtual address space. */
	struct page_table *task_pml4;

	/* The VMAs */
	struct rb_tree task_rb;
	struct list task_mmap;

	/* The children */
	struct list task_children; /* Own children */
	struct list task_child; /* The node in the parent's list */

	/* The zombies */
	struct list task_zombies;

	/* The anchor node (for zombies) */
	struct list task_node;

	/* Where to store the exit status of the task we are waiting on. */
	int *task_rstatus;

#ifndef USE_BIG_KERNEL_LOCK
	/* Per-task lock */
	struct spinlock task_lock;
#endif
};
