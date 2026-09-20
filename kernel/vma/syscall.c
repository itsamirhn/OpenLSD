
#include <types.h>

#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/vma.h>

#include <lib.h>

int sys_mquery(struct vma_info *info, void *addr)
{
	struct vma *vma;
	struct list *node;
	physaddr_t *entry;

	/* Check if the user has read/write access to the info struct. */
	assert_user_mem(cur_task, info, sizeof *info, PROT_WRITE);

	/* Do not leak information about the kernel space. */
	if (addr >= (void *)USER_LIM) {
		return -1;
	}

	/* Clear the info struct. */
	memset(info, 0, sizeof *info);

	/* Find the VMA with an end address that is greater than the requested
	 * address, but also the closest to the requested address.
	 */
	vma = find_vma(&cur_task->task_rb, addr);

	if (!vma) {
		/* If there is no such VMA, it means the address is greater
		 * than the address of any VMA in the address space, i.e. the
		 * user is requesting the free gap at the end of the address
		 * space. The base address of this free gap is the end address
		 * of the highest VMA and the end address is simply USER_LIM.
		 */
		node = list_tail(&cur_task->task_mmap);

		info->vm_end = (void *)USER_LIM;

		if (!node) {
			return 0;
		}

		vma = container_of(node, struct vma, vm_mmap);
		info->vm_base = vma->vm_src ? (void *)ROUNDUP((uintptr_t)vma->vm_end, PAGE_SIZE) : vma->vm_end;

		return 0;
	}

	if (addr < vma->vm_base && !(vma->vm_src && addr >= (void *)ROUNDDOWN((uintptr_t)vma->vm_base, PAGE_SIZE))) {
		/* The address lies outside the found VMA. This means the user
		 * is requesting the free gap between two VMAs. The base
		 * address of the free gap is the end address of the previous
		 * VMA. The end address of the free gap is the base address of
		 * the VMA that we found.
		 */
		node = list_prev(&cur_task->task_mmap, &vma->vm_mmap);

		info->vm_end = vma->vm_src? (void *)ROUNDDOWN((uintptr_t)vma->vm_base, PAGE_SIZE) : vma->vm_base;

		if (!node) {
			return 0;
		}

		vma = container_of(node, struct vma, vm_mmap);
		info->vm_base = vma->vm_src ? (void *)ROUNDUP((uintptr_t)vma->vm_end, PAGE_SIZE) : vma->vm_end;

		return 0;
	}

	/* The requested address actually lies within a VMA. Copy the
	 * information.
	 */
	strncpy(info->vm_name, vma->vm_name, 64);
	info->vm_base = vma->vm_src ? (void *)ROUNDDOWN((uintptr_t)vma->vm_base, PAGE_SIZE) : vma->vm_base;
	info->vm_end = vma->vm_src ? (void *)ROUNDUP((uintptr_t)vma->vm_end, PAGE_SIZE) : vma->vm_end;
	info->vm_prot = vma->vm_flags;
	info->vm_type = vma->vm_src ? VMA_EXECUTABLE : VMA_ANONYMOUS;

	/* Check if the address is backed by a physical page. */
	if (page_lookup(cur_task->task_pml4, addr, &entry)) {
		info->vm_mapped = (*entry & PAGE_HUGE) ? VM_2M_PAGE : VM_4K_PAGE;
	}

	return 0;
}

void *sys_mmap(void *addr, size_t len, int prot, int flags, int fd,
	uintptr_t offset)
{
	const int supported_flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED | MAP_POPULATE;
	uintptr_t start;
	uintptr_t end;
	int vma_flags = 0;
	struct vma *vma;

	if (!len || (flags & supported_flags) != flags || fd != -1 || offset != 0 || (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))) {
		return MAP_FAILED;
	}
	if ((prot & PROT_WRITE) && !(prot & PROT_READ)) {
		return MAP_FAILED;
	}
	if ((prot & PROT_EXEC) && !(prot & PROT_READ)) {
		return MAP_FAILED;
	}

	start = (uintptr_t)addr;
	if (flags & MAP_FIXED) {
		if (!addr || !page_aligned(start)) {
			return MAP_FAILED;
		}
	} else {
		start = ROUNDDOWN(start, PAGE_SIZE);
	}
	if (start + len < start) { // Check for overflow
		return MAP_FAILED;
	}
	end = ROUNDUP(start + len, PAGE_SIZE);
	if (end > USER_LIM) {
		return MAP_FAILED;
	}

	if (prot & PROT_READ){
		vma_flags |= VM_READ;
	}
	if (prot & PROT_WRITE){
		vma_flags |= VM_WRITE;
	}
	if (prot & PROT_EXEC){
		vma_flags |= VM_EXEC;
	}
	if (flags & MAP_FIXED) {
		unmap_and_remove_vma_range(cur_task, (void *)start, len);
		vma = add_anonymous_vma(cur_task, "user", (void *)start,len, vma_flags);
	} else {
		vma = add_vma(cur_task, "user", addr, len, vma_flags);
	}
	if (!vma) {
		return MAP_FAILED;
	}

	if (flags & MAP_POPULATE && populate_vma_range(cur_task, vma->vm_base,(uintptr_t)vma->vm_end - (uintptr_t)vma->vm_base, vma_flags) < 0) {
		unmap_and_remove_vma_range(cur_task, vma->vm_base,(uintptr_t)vma->vm_end - (uintptr_t)vma->vm_base);
		return MAP_FAILED;
	}

	return vma->vm_base;
}

void sys_munmap(void *addr, size_t len)
{
	uintptr_t start;
	uintptr_t end;

	if (!len) {
		return;
	}
	start = ROUNDDOWN((uintptr_t)addr, PAGE_SIZE);
	if (start + len < start) { // Check for overflow
		return;
	}
	end = ROUNDUP(start + len, PAGE_SIZE);
	if (start >= USER_LIM || end > USER_LIM) {
		return;
	}
	unmap_and_remove_vma_range(cur_task, (void *)start, len);
}

int sys_mprotect(void *addr, size_t len, int prot)
{
	/* LAB 4: your code here. */
	return -ENOSYS;
}

int sys_madvise(void *addr, size_t len, int advise)
{
	/* LAB 4: your code here. */
	return -ENOSYS;
}
