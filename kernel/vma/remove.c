
#include <task.h>
#include <vma.h>
#include <list.h>

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
	struct list *node, *next;

	if (!task) {
		return;
	}

	list_foreach_safe(&task->task_mmap, node, next) {
		struct vma *vma = container_of(node, struct vma, vm_mmap);

		remove_vma(task, vma);
		kfree(vma);
	}
}

/* Splits the VMA into the address range [base, base + size) and removes the
 * resulting VMA and any physical pages that back the VMA.
 */
static int remove_vma_callback(struct task *task, void *base, size_t size, struct vma **vma,
	void *udata)
{
	struct vma *current = *vma;
	uintptr_t remove_start = (uintptr_t)base;
	uintptr_t remove_end = remove_start + size;
	uintptr_t original_end = (uintptr_t)current->vm_end;
	uintptr_t left_end = remove_start > (uintptr_t)current->vm_base ? remove_start : (uintptr_t)current->vm_base;
	uintptr_t right_start = remove_end < (uintptr_t)current->vm_end ? remove_end : (uintptr_t)current->vm_end;
	struct list *next_node;

	if (remove_start < (uintptr_t)current->vm_end && remove_end > (uintptr_t)current->vm_base) {
		unmap_page_range(task->task_pml4, (void *)left_end,right_start - left_end);
	}

	if (left_end == (uintptr_t)current->vm_base && right_start == (uintptr_t)current->vm_end) {
		next_node = list_next(&task->task_mmap, &current->vm_mmap);
		remove_vma(task, current);
		kfree(current);
		*vma = next_node ? container_of(next_node, struct vma, vm_mmap) : NULL;
		return 0;
	}

	if (left_end != (uintptr_t)current->vm_base) {
		current->vm_end = (void *)left_end;
	}

	if (right_start < original_end && left_end != (uintptr_t)current->vm_base) {
		struct vma *right = kmalloc(sizeof *right);
		if (!right) {
			return -1;
		}
		*right = *current;
		list_init(&right->vm_mmap);
		rb_node_init(&right->vm_rb);
		right->vm_base = (void *)right_start;
		right->vm_end = (void *)original_end;
		if (right->vm_src) {
			right->vm_offset += right_start - (uintptr_t)current->vm_base;
			right->vm_len -= right_start - (uintptr_t)current->vm_base;
		}
		current->vm_end = (void *)left_end;
		if (insert_vma(task, right) < 0) {
			kfree(right);
			return -1;
		}
	} else if (right_start < original_end) {
		uintptr_t old_base = (uintptr_t)current->vm_base;
		current->vm_base = (void *)right_start;
		if (current->vm_src) {
			current->vm_offset += right_start - old_base;
			current->vm_len -= right_start - old_base;
		}
	}

	return 0;
}

/* Unmaps pages and removes the VMAs for the given address range
 * [base, base + size).
 */
int unmap_and_remove_vma_range(struct task *task, void *base, size_t size)
{
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
