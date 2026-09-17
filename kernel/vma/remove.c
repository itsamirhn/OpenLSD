
#include <task.h>
#include <vma.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Removes the given VMA from the given task. */
void remove_vma(struct task *task, struct vma *vma)
{
	if (!task || !vma) {
		return;
	}

	rb_remove(&task->task_rb, &vma->vm_rb);
	rb_node_init(&vma->vm_rb);
	list_del(&vma->vm_mmap);
}

/* Frees all the VMAs for the given task. */
void free_all_vmas(struct task *task)
{
	/* LAB 4: your code here. */
}

/* Splits the VMA into the address range [base, base + size) and removes the
 * resulting VMA and any physical pages that back the VMA.
 */
static int remove_vma_callback(struct task *task, void *base, size_t size, struct vma **vma,
	void *udata)
{
	/* LAB 4: your code here. */
	return 0;
}

/* Unmaps pages and removes the VMAs for the given address range
 * [base, base + size).
 */
int unmap_and_remove_vma_range(struct task *task, void *base, size_t size)
{
	show_vmas(task);
	return walk_vma_range(task, base, size, remove_vma_callback, NULL);
}

/* Unmaps any non-dirty physical pages for the given address range
 * [base, base + size) within the VMA.
 */
static int unmap_clean_pages_callback(struct task *task, void *base, size_t size, struct vma **vma,
	void *udata)
{
	/* LAB 4: your code here. */
	return 0;
}

/* Unmaps any non-dirty physical pages within the address range
 * [base, base + size).
 */
int unmap_clean_pages_range(struct task *task, void *base, size_t size)
{
	return walk_vma_range(task, base, size, unmap_clean_pages_callback, NULL);
}

