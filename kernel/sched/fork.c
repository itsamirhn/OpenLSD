
#include <error.h>
#include <list.h>

#include <kernel/console.h>
#include <kernel/mem.h>
#include <kernel/monitor.h>
#include <kernel/sched.h>
#include <kernel/vma.h>

struct cow_info {
	struct page_table *pml4;
	struct page_table *child_pml4;
};

static int cow_share_pte(physaddr_t *entry, uintptr_t base, uintptr_t end, struct page_walker *walker) {
	struct cow_info *info = walker->udata;

	if (!(*entry & PAGE_PRESENT)) return 0;

	if (*entry & PAGE_WRITE) {
		*entry &= ~PAGE_WRITE;
		tlb_invalidate(info->pml4, (void *)base);
	}

	return page_insert(info->child_pml4, pa2page(PAGE_ADDR(*entry)), (void *)base, *entry & (PAGE_UMASK | PAGE_DIRTY));
}

static int cow_share_pde(physaddr_t *entry, uintptr_t pde_base, uintptr_t pde_end, struct page_walker *walker) {
	if (!(*entry & PAGE_HUGE)) return 0;
	return cow_share_pte(entry, pde_base, pde_end, walker);
}

/* Allocates a task struct for the child process and copies the register state,
 * the VMAs and the page tables. Once the child task has been set up, it is
 * added to the run queue.
 */
struct task *task_clone(struct task *task)
{
	struct task *child = task_alloc(task->task_pid);
	if (!child) return NULL;

	memcpy(&child->task_frame, &task->task_frame, sizeof child->task_frame);

	struct list *node;
	list_foreach(&task->task_mmap, node) {
		struct vma *vma = container_of(node, struct vma, vm_mmap);
		assert(add_executable_vma(child, vma->vm_name, vma->vm_base, vma->vm_end - vma->vm_base,vma->vm_flags, vma->vm_src, vma->vm_len, vma->vm_offset) != NULL);
	}

	assert(walk_user_pages(task->task_pml4, &(struct page_walker) {
		.pte_callback = cow_share_pte,
		.pde_callback = cow_share_pde,
		.udata = &(struct cow_info) {
			.pml4 = task->task_pml4,
			.child_pml4 = child->task_pml4
		}
	}) == 0);

	return child;
}

pid_t sys_fork(void)
{
	struct task *task = task_clone(cur_task);
	if (!task) return -ENOMEM;

	task->task_frame.rax = 0;
	task->task_karma = cur_task->task_karma + read_tsc() - cur_task->task_start_tsc;
	list_add(&cur_task->task_children, &task->task_child);

	sched_enqueue(task);

	return task->task_pid;
}
