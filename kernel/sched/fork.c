
#include <error.h>
#include <list.h>

#include <kernel/console.h>
#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/vma.h>

/* Allocates a task struct for the child process and copies the register state,
 * the VMAs and the page tables. Once the child task has been set up, it is
 * added to the run queue.
 */
struct task *task_clone(struct task *task)
{
	struct task *child = task_alloc(task->task_pid);
	if (!child) return NULL;

	memcpy(&child->task_frame, &task->task_frame, sizeof child->task_frame);

	/* LAB 5: your code here. */
	// Need to copy the VMAs

	return child;
}

pid_t sys_fork(void)
{
	struct task *task = task_clone(cur_task);
	if (!task) return -ENOMEM;
	return task->task_pid;
}
