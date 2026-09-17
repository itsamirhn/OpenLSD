
#include <task.h>
#include <vma.h>

#include <kernel/vma.h>

/* Finds the VMA in the red-black tree with an end address greater than the
 * requested address. If the base address of any found VMA is less than the
 * requested address, then the VMA is an exact match and that VMA is returned
 * directly.
 */
struct vma *find_vma(struct rb_tree *tree, void *addr)
{
	struct rb_node *node, *parent = NULL;
	struct vma *vma = NULL;
	struct vma *vma_tmp = NULL;
	int cmp, dir = 0;

	node = tree->root;

	while (node) {
		vma_tmp = container_of(node, struct vma, vm_rb);
		parent = node;
		dir = (addr >= vma_tmp->vm_end);

		if (!dir) {
			vma = vma_tmp;

			if (vma_tmp->vm_base <= addr) {
				break;
			}
		}

		node = dir ? node->right : node->left;
	}

	return vma;
}

/* Given a task and an address, this function finds the VMA that the address
 * belongs to. Otherwise this function returns NULL if no VMA is found.
 */
struct vma *task_find_vma(struct task *task, void *addr)
{
	struct vma *vma;

	vma = find_vma(&task->task_rb, addr);

	if (!vma || addr < vma->vm_base) {
		return NULL;
	}

	return vma;
}

