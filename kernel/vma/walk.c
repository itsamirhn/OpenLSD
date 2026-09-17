
#include <task.h>
#include <vma.h>

#include <kernel/vma.h>

/* Walks over the address range [base, base + size) for the given task,
 * iterating over all the VMAs that fall within that address range and calling
 * the user-provided get_vma() function for each VMA.
 *
 * The callback gets a double pointer to the VMA - in case the original passed
 * VMA is invalidated (e.g. due to a merge or deletion), a different, valid VMA
 * should be stored to ensure the walker continues its walk safely.
 *
 * If get_vma() involves a deletion operation, it will free the original vma
 * and update the *vma pointer to the next vma. Because the pointer has already
 * advanced, the manual iteration step is skipped. Other operations, such as a 
 * potential merge, will update the pointer to the original or a previous vma. 
 * In those cases, the iteration step runs normally to fetch the next node.
 */
int walk_vma_range(struct task *task, void *base, size_t size,
	int (* get_vma)(struct task *, void *, size_t, struct vma **, void *),
	void *udata)
{
	struct vma *vma;
	struct list *node;
	void *end = (void *)((uintptr_t)base + size);
	int ret;

	/* Find the VMA with an end address greater than the base, but also the
	 * closest to the base.
	 */
	vma = find_vma(&task->task_rb, base);

	if (!vma || end <= vma->vm_base) {
		return -1;
	}

	/* Now call get_vma() on each VMA as long as there is a VMA and as long
	 * as each VMA has a base address less than the end address.
	 */
	while (vma && vma->vm_base < end) {
		void *old_base = vma->vm_base;
		ret = get_vma(task, base, size, &vma, udata);

		if (ret < 0) {
			return ret;
		}

		/* If we didn't jump forward (e.g. deletion), safely fetch next */
		if (vma && vma->vm_base <= old_base) {
			node = list_next(&task->task_mmap, &vma->vm_mmap);
			vma = node ? container_of(node, struct vma, vm_mmap) : NULL;
		}
	}

	return 0;
}

