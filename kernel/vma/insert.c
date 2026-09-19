#include <types.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

static inline int vma_validate(struct vma *new_vma, struct vma *curr_vma)
{
	/* Check for overlap */
	/* If new start < curr end AND new end > curr start -> Overlap */
	if (new_vma->vm_base < curr_vma->vm_end && new_vma->vm_end > curr_vma->vm_base) {
		return -1; /* Fail */
	}
	return 0; /* Pass */
}

/** The necessary functions for the RB tree */
static inline int vma_compare(struct vma *a, struct vma *b)
{
	if (a->vm_base < b->vm_base) {
		return -1;
	} else if (a->vm_base > b->vm_base) {
		return 1;
	}

	return 0;
}

RB_DEFINE_GUARDED_INSERT_FUNC(
    struct vma,
    rb_vma_guarded_insert,
    vma_compare,
    vm_rb,
    vma_validate)


/* Inserts the given VMA into the red-black tree of the given task. First tries
 * to find a VMA for the end address of the given end address. If there is
 * already a VMA that overlaps, this function returns -1. Then the VMA is
 * inserted into the red-black tree and added to the sorted linked list of
 * VMAs.
 */
int insert_vma(struct task *task, struct vma *vma)
{
	struct vma *vma_next = NULL;

	if (rb_vma_guarded_insert(&task->task_rb, vma, &vma_next) < 0) {
		return -1;
	}

	/* The successor from the tree insertion tells us where to insert in the
	 * linked list. If there's no successor, we insert at the end.
	 */
	if (!vma_next) {
		/* No successor, insert at end of list */
		list_insert_before(&task->task_mmap, &vma->vm_mmap);
	} else {
		/* Insert before the successor */
		list_insert_before(&vma_next->vm_mmap, &vma->vm_mmap);
	}

	return 0;
}

/* Allocates and adds a new VMA for the given task.
 *
 * This function first allocates a new VMA. Then it copies over the given
 * information. The VMA is then inserted into the red-black tree and linked
 * list. Finally, this functions attempts to merge the VMA with the adjacent
 * VMAs.
 *
 * Returns the new VMA if it could be added, NULL otherwise.
 */
struct vma *add_executable_vma(struct task *task, char *name, void *addr,
	size_t size, int flags, void *src, size_t len, size_t src_offset)
{
	struct vma *rhs = NULL;
	struct vma *lhs = NULL;
	struct vma *vma = kmalloc(sizeof(struct vma));
	if(!vma) { return NULL; }

	list_init(&vma->vm_mmap);
	vma->vm_base = addr;
	vma->vm_end = addr + size;
	vma->vm_flags = flags;
	vma->vm_name = name;
	vma->vm_src = src;
	vma->vm_len = len;
	vma->vm_offset = src_offset;

	if (insert_vma(task, vma) < 0) {
		kfree(vma);
		return NULL;
	}
	vma = merge_vmas(task, vma);

	return vma;
}

/* A simplified wrapper to add anonymous VMAs, i.e. VMAs not backed by an
 * executable.
 */
struct vma *add_anonymous_vma(struct task *task, char *name, void *addr,
	size_t size, int flags)
{
	return add_executable_vma(task, name, addr, size, flags, NULL, 0, 0);
}

/* Allocates and adds a new VMA to the requested address or tries to find a
 * suitable free space that is sufficiently large to host the new VMA. If the
 * address is NULL, this function scans the address space from the end to the
 * beginning for such a space. If an address is given, this function scans the
 * address space from the given address to the beginning and then scans from
 * the end to the given address for such a space.
 *
 * Returns the VMA if it could be added. NULL otherwise.
 */
struct vma *add_vma(struct task *task, char *name, void *addr, size_t size,
	int flags)
{
	/* LAB 4: your code here. */
	return NULL;
}
