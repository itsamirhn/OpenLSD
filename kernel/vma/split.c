
#include <task.h>
#include <vma.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

/* Given a task and a VMA, this function splits the VMA at the given address
 * by setting the end address of original VMA to the given address and by
 * adding a new VMA with the given address as base.
 */
struct vma *split_vma(struct task *task, struct vma *lhs, void *addr)
{
	struct vma *rhs;
	uintptr_t split_addr = (uintptr_t)addr;
	uintptr_t lhs_base = (uintptr_t)lhs->vm_base;
	uintptr_t old_end = (uintptr_t)lhs->vm_end;
	uintptr_t delta = split_addr - lhs_base;

	if (!task || !lhs || split_addr <= lhs_base || split_addr >= old_end) {
		return NULL;
	}

	rhs = kmalloc(sizeof *rhs);
	if (!rhs) return NULL;

	*rhs = *lhs;
	list_init(&rhs->vm_mmap);
	rb_node_init(&rhs->vm_rb);
	rhs->vm_base = addr;
	if (rhs->vm_src) {
		rhs->vm_offset += delta;
		if (delta >= rhs->vm_len) {
			rhs->vm_len = 0;
		} else {
			rhs->vm_len -= delta;
		}
		lhs->vm_len = MIN(lhs->vm_len, delta);
	}
	lhs->vm_end = addr;

	if (insert_vma(task, rhs) < 0) {
		kfree(rhs);
		return NULL;
	}

	return rhs;
}

/* Given a task and a VMA, this function first splits the VMA into a left-hand
 * and right-hand side at address base. Then this function splits the
 * right-hand side or the original VMA, if no split happened, into a left-hand
 * and a right-hand side. This function finally returns the right-hand side of
 * the first split or the original VMA.
 */
struct vma *split_vmas(struct task *task, struct vma *vma, void *base, size_t size)
{
	uintptr_t start = (uintptr_t)base;
	uintptr_t end = start + size;
	struct vma *split;

	if (!task || !vma || !size || end < start) return vma;

	if (base > vma->vm_base && base < vma->vm_end) {
		split = split_vma(task, vma, base);
		if (!split) return vma;
		vma = split;
	}

	if (end > (uintptr_t)vma->vm_base && end < (uintptr_t)vma->vm_end) {
		split_vma(task, vma, (void *)end);
	}

	return vma;
}
