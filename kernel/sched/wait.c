
#include <types.h>
#include <error.h>
#include <lib.h>

#include <kernel/mem.h>
#include <kernel/sched.h>


pid_t sys_wait(int *rstatus)
{
	return sys_waitpid(-1, rstatus, 0);
}

pid_t sys_waitpid(pid_t pid, int *rstatus, int opts)
{
	struct list	*node;

	list_foreach(&cur_task->task_zombies, node) {
		struct task *task = container_of(node, struct task, task_node);
		if (pid > 0 && task->task_pid != pid) continue;
		if (rstatus) *rstatus = task->task_exit_status;
		pid = task->task_pid;
		task_free(task);
		return pid;
	}

	list_foreach(&cur_task->task_children, node) {
		struct task *task = container_of(node, struct task, task_child);
		if (pid > 0 && task->task_pid != pid) continue;
		cur_task->task_wait = pid > 0 ? task : NULL;
		cur_task->task_status = TASK_NOT_RUNNABLE;
		sched_yield();
	}

	return -ECHILD;
}
