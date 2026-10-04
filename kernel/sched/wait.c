
#include <types.h>
#include <cpu.h>
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
	struct task *self = cur_task;

	if (rstatus) assert_user_mem(self, rstatus, sizeof *rstatus, PROT_WRITE);

	fine_spin_lock(&self->task_lock);
	list_foreach(&self->task_zombies, node) {
		struct task *task = container_of(node, struct task, task_node);
		if (pid > 0 && task->task_pid != pid) continue;
		if (rstatus) *rstatus = task->task_exit_status;
		pid = task->task_pid;
		cprintf("[PID %5u] Reaping task with PID %u\n", self->task_pid, task->task_pid);
		task_free(task);
		fine_spin_unlock(&self->task_lock);
		return pid;
	}

	list_foreach(&self->task_children, node) {
		struct task *task = container_of(node, struct task, task_child);
		if (pid > 0 && task->task_pid != pid) continue;
		self->task_wait = pid > 0 ? task : NULL;
		self->task_status = TASK_NOT_RUNNABLE;
		self->task_rstatus = rstatus;
		self->task_karma += read_tsc() - self->task_start_tsc;
		cur_task = NULL;
		load_pml4(PADDR(kernel_pml4));
		fine_spin_unlock(&self->task_lock);
		sched_yield();
	}

	fine_spin_unlock(&self->task_lock);
	return -ECHILD;
}
