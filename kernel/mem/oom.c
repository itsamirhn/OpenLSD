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

static int rss_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if (*entry & PAGE_PRESENT) *(size_t *)walker->udata += 1;
	return 0;
}

static int rss_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	if ((*entry & PAGE_PRESENT) && (*entry & PAGE_HUGE)) *(size_t *)walker->udata += HPAGE_SIZE / PAGE_SIZE;
	return 0;
}

// The resident set size of a task in pages.
size_t task_rss(struct task *task)
{
	size_t rss = 0;
	struct page_walker walker = {
		.pte_callback = rss_pte,
		.pde_callback = rss_pde,
		.udata = &rss,
	};

	walk_user_pages(task->task_pml4, &walker);
	return rss;
}

// Kills the user task with the largest RSS
bool oom_kill(void)
{
	struct task *task, *victim = NULL;
	size_t rss, victim_rss = 0;
	pid_t pid;

	// We've got killed in the meanwhile by other core, so yield
	if (cur_task && cur_task->task_killed) sched_yield();

	if (!spin_trylock(&oom_lock)) return true;

	for (pid = 1; pid < pid_max; ++pid) {
		task = tasks[pid];
		if (!task || task->task_type != TASK_TYPE_USER || task->task_killed || task->task_status == TASK_DYING) continue; // let kernel tasks live

		rss = task_rss(task);
		if (rss > victim_rss) { // find max rss
			victim = task;
			victim_rss = rss;
		}
	}

	if (victim) {
		cprintf("[PID %5u] Out of memory: killing task %u with an RSS of %u KiB\n", cur_task ? cur_task->task_pid : 0, victim->task_pid, victim_rss * 4);
		if (victim == cur_task) spin_unlock(&oom_lock);
		task_kill(victim);
		// flush what we freed
		page_zero_flush();
	}

	spin_unlock(&oom_lock);
	return victim != NULL;
}
