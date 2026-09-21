
#include <task.h>
#include <vma.h>
#include <list.h>

#include <kernel/mem.h>
#include <kernel/vma.h>

struct clean_page_info {
	struct page_table *pml4;
	uintptr_t base;
	uintptr_t end;
};

static int remove_clean_pte(physaddr_t *entry, uintptr_t page_base,
	uintptr_t page_end, struct page_walker *walker)
{
	struct clean_page_info *info = walker->udata;
	struct page_info *page = pa2page(PAGE_ADDR(*entry));

	if (!(*entry & PAGE_PRESENT) || (*entry & PAGE_DIRTY)) return 0;
	*entry = PAGE_NONPRESENT(page2pa(page));
	tlb_invalidate(info->pml4, (void *)page_base);
	page_decref(page);
	return 0;
}

static int remove_clean_pde(physaddr_t *entry, uintptr_t pde_base,
	uintptr_t pde_end, struct page_walker *walker)
{
	struct clean_page_info *info = walker->udata;
	struct page_info *page = pa2page(PAGE_ADDR(*entry));

	if (!(*entry & PAGE_PRESENT) || !(*entry & PAGE_HUGE)) return 0;
	if (*entry & PAGE_DIRTY) return 0;
	if (info->base <= pde_base && pde_end <= info->end) {
		*entry = PAGE_NONPRESENT(page2pa(page));
		tlb_invalidate(info->pml4, (void *)pde_base);
		page_decref(page);
		return 0;
	}
	return ptbl_split(entry, pde_base, pde_end, walker);
}

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
	uintptr_t start = remove_start > (uintptr_t)current->vm_base ? remove_start : (uintptr_t)current->vm_base;
	uintptr_t end = remove_end < (uintptr_t)current->vm_end ? remove_end : (uintptr_t)current->vm_end;
	struct vma *target;
	struct list *next_node;

	if (remove_end < remove_start || remove_start >= (uintptr_t)current->vm_end || remove_end <= (uintptr_t)current->vm_base) {
		return 0;
	}

	target = split_vmas(task, current, (void *)start, end - start);
	if (base > current->vm_base && target == current){
		return -1; //split_vmas failed
	} 

	unmap_page_range(task->task_pml4, (void *)start, end - start);
	next_node = list_next(&task->task_mmap, &target->vm_mmap);
	remove_vma(task, target);
	kfree(target);
	*vma = next_node ? container_of(next_node, struct vma, vm_mmap) : NULL;

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
	struct clean_page_info info = {
		.pml4 = task->task_pml4,
		.base = ROUNDDOWN((uintptr_t)base, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)base + size, PAGE_SIZE) - 1,
	};
	uintptr_t start = (uintptr_t)base;
	uintptr_t end = start + size;
	struct page_walker walker = {
		.pte_callback = remove_clean_pte,
		.pde_callback = remove_clean_pde,
		.pde_unmap = ptbl_free,
		.pdpte_unmap = ptbl_free,
		.pml4e_unmap = ptbl_free,
		.udata = &info,
	};

	return walk_page_range(info.pml4, base, (void *)end, &walker);
}

/* Unmaps any non-dirty physical pages within the address range
 * [base, base + size).
 */
int unmap_clean_pages_range(struct task *task, void *base, size_t size)
{
	return walk_vma_range(task, base, size, unmap_clean_pages_callback, NULL);
}
