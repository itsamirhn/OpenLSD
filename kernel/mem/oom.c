#include <types.h>
#include <paging.h>
#include <spinlock.h>
#include <task.h>
#include <cpu.h>

#include <kernel/mem.h>
#include <kernel/sched.h>

extern pid_t pid_max;
extern struct task **tasks;

static struct spinlock oom_lock = {
	.rank = RANK_OOM,
#ifdef DEBUG_SPINLOCK
	.name = "oom_lock",
#endif
};

static int footprint_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if ((*entry & PAGE_PRESENT) || pte_swapped(*entry)) *(size_t *)walker->udata += 1;
	return 0;
}

static int footprint_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if (((*entry & PAGE_PRESENT) && (*entry & PAGE_HUGE)) || pte_swapped(*entry)) *(size_t *)walker->udata += HPAGE_SIZE / PAGE_SIZE;
	return 0;
}

// RSS + swap pages of a task
size_t task_footprint(struct task *task)
{
	size_t pages = 0;
	struct page_walker walker = {
		.pte_callback = footprint_pte,
		.pde_callback = footprint_pde,
		.udata = &pages,
	};

	walk_user_pages(task->task_pml4, &walker);
	return pages;
}

// Kills the user task with the largest footprint
bool oom_kill(void)
{
	struct task *task, *victim = NULL;
	size_t pages, victim_pages = 0;
	pid_t pid;

	// We've got killed in the meanwhile by other core, so yield
	if (cur_task && cur_task->task_killed) sched_yield();

	if (!spin_trylock(&oom_lock)) return true;

	for (pid = 1; pid < pid_max; ++pid) {
		task = tasks[pid];
		if (!task || task->task_type != TASK_TYPE_USER || task->task_killed || task->task_status == TASK_DYING) continue; // let kernel tasks live

		pages = task_footprint(task);
		if (pages > victim_pages) { // find max footprint
			victim = task;
			victim_pages = pages;
		}
	}

	if (victim) {
		cprintf("[PID %5u] Out of memory: killing task %u using %u KiB\n", cur_task ? cur_task->task_pid : 0, victim->task_pid, victim_pages * 4);
		if (victim == cur_task) spin_unlock(&oom_lock);
		task_kill(victim);
		// flush what we freed
		page_zero_flush();
	}

	spin_unlock(&oom_lock);
	return victim != NULL;
}
