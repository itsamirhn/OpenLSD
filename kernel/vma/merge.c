
#include <task.h>
#include <vma.h>

#include <kernel/vma.h>
#include <kernel/mem.h>

/* Given a task and two VMAs, checks if the VMAs are adjacent and compatible
 * for merging. If they are, then the VMAs are merged by removing the
 * right-hand side and extending the left-hand side by setting the end address
 * of the left-hand side to the end address of the right-hand side.
 */
struct vma *merge_vma(struct task *task, struct vma *lhs, struct vma *rhs)
{
	if (lhs->vm_end != rhs->vm_base || lhs->vm_flags != rhs->vm_flags || lhs->vm_src != rhs->vm_src) {
		return NULL;
	}

	if (lhs->vm_src && lhs->vm_offset + lhs->vm_len != rhs->vm_offset) {
		return NULL;
	}

	lhs->vm_end = rhs->vm_end;
	lhs->vm_len += rhs->vm_len;
	remove_vma(task, rhs);
	kfree(rhs);
	return lhs;
}

/* Given a task and a VMA, this function attempts to merge the given VMA with
 * the previous and the next VMA. Returns the merged VMA or the original VMA if
 * the VMAs could not be merged.
 */
struct vma *merge_vmas(struct task *task, struct vma *vma)
{
	struct vma *lhs = NULL;
	struct vma *rhs = NULL;

	if(vma->vm_base) { 	lhs = task_find_vma(task, vma->vm_base - 1); }

	rhs = task_find_vma(task, vma->vm_end + 1);

	if(lhs != NULL){
		lhs = merge_vma(task, lhs, vma);
		vma = lhs ? lhs : vma;
	} 
	if(rhs != NULL){
		rhs = merge_vma(task, vma, rhs);
		vma = rhs ? rhs : vma;
	} 
	return vma;
}
