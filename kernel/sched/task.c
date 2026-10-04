
#include <error.h>
#include <string.h>
#include <paging.h>
#include <task.h>
#include <cpu.h>
#include <lib.h>
#include <atomic.h>
#ifdef BONUS_VDSO
#include <vdso.h>
#endif

#include <kernel/monitor.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/vma.h>

#ifdef USE_BIG_KERNEL_LOCK
extern struct spinlock kernel_lock;
#endif

pid_t pid_max = 1 << 16;
struct task **tasks = (struct task **)PIDMAP_BASE;
size_t nuser_tasks = 0;

/* Looks up the respective task for a given PID.
 * If check_perm is non-zero, this function checks if the PID maps to the
 * current task or if the current task is the parent of the task that the PID
 * maps to.
 *
 * If pid is zero, this will return the current process
 */
struct task *pid2task(pid_t pid, int check_perm)
{
	struct task *task;

	/* PID 0 is the current task. */
	if (pid == 0) {
		return cur_task;
	}

	/* Limit the PID. */
	if (pid >= pid_max) {
		return NULL;
	}

	/* Look up the task in the PID map. */
	task = tasks[pid];

	/* No such mapping found. */
	if (!task) {
		return NULL;
	}

	/* If we don't have to do a permission check, we can simply return the
	 * task.
	 */
	if (!check_perm) {
		return task;
	}

	/* Check if the task is the current task or if the current task is the
	 * parent. If not, then the current task has insufficient permissions.
	 */
	if (task != cur_task && task->task_ppid != cur_task->task_pid) {
		return NULL;
	}

	return task;
}

void task_init(void)
{
	/* Allocate an array of pointers at PIDMAP_BASE to be able to map PIDs
	 * to tasks.
	 */

	size_t size = pid_max * sizeof(struct task *);
	populate_region(kernel_pml4, (void *)PIDMAP_BASE, size, PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC);
	memset(tasks, 0, size);
}

/* Sets up the virtual address space for the task. */
static int task_setup_vas(struct task *task)
{
	struct page_info *page;

	/* Allocate a page for the page table. */
	page = page_alloc(ALLOC_ZERO);

	if (!page) {
		return -ENOMEM;
	}

	++page->pp_ref;

	/* Now set task->task_pml4 and initialize the page table.
	 * Can you use kernel_pml4 as a template?
	 */

	task->task_pml4 = page2kva(page);

	// Just copy the kernel space mapping and leave the user space mapping empty
	for (size_t i = PML4_INDEX(KERNEL_VMA); i < PAGE_TABLE_ENTRIES; i++) task->task_pml4->entries[i] = kernel_pml4->entries[i];

	return 0;
}

#ifdef BONUS_VDSO
extern uint8_t vdso_blob_start[];
extern uint8_t vdso_blob_end[];

static int task_map_vdso(struct task *task, uintptr_t vdso_base) {
	struct elf *ehdr = (struct elf *)vdso_blob_start;
	struct elf_proghdr *phdr;
	size_t i;

	assert(ehdr->e_magic == ELF_MAGIC);

	phdr = (struct elf_proghdr *)(vdso_blob_start + ehdr->e_phoff);

	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		uintptr_t va, base, end;
		int flags = VM_READ;

		if (phdr->p_type != ELF_PROG_LOAD) continue;
		if (phdr->p_flags & ELF_PROG_FLAG_EXEC) flags |= VM_EXEC;

		va = vdso_base + phdr->p_va;
		base = ROUNDDOWN(va, PAGE_SIZE);
		end = ROUNDUP(va + phdr->p_memsz, PAGE_SIZE);

		assert(end <= vdso_base + VDSO_MAX_PAGES * PAGE_SIZE);

		assert(add_executable_vma(task, "vdso", (void *)base, end - base, flags, vdso_blob_start + phdr->p_offset, phdr->p_filesz, va - base) != NULL);
	}

	return 0;
}

static int task_setup_vdso(struct task *task) {

	struct page_info *page = page_alloc(ALLOC_ZERO);
	if (!page) return -ENOMEM;

	uintptr_t vvar_base = VVAR_BASE - (read_tsc() % VDSO_RANDOM_SLOTS) * PAGE_SIZE;
	// uintptr_t vdso_base = VDSO_BASE - (read_tsc() % VDSO_RANDOM_SLOTS) * PAGE_SIZE;
	uintptr_t vdso_base = vvar_base - VDSO_MAX_PAGES * PAGE_SIZE;

	struct vdso_data *vdso_data = (struct vdso_data *)page2kva(page);
	vdso_data->pid = task->task_pid;
	vdso_data->vdso_base = vdso_base;
	vdso_data->tsc_base = time_tsc_base();
	vdso_data->tsc_khz = time_tsc_khz();
	vdso_data->epoch_base = time_epoch_base();
	
	// It should have better error handling ... but assertions are fine for now
	assert(page_insert(task->task_pml4, page, (void *)vvar_base, PAGE_PRESENT | PAGE_USER | PAGE_NO_EXEC) == 0);
	assert(add_anonymous_vma(task, "vvar", (void *)vvar_base, PAGE_SIZE, VM_READ) != NULL);
	task->task_frame.r12 = vvar_base;

	task_map_vdso(task, vdso_base);

	return 0;
}
#endif

/* Allocates and initializes a new task.
 * On success, the new task is returned.
 */
struct task *task_alloc(pid_t ppid)
{
	struct task *task;
	pid_t pid;

	/* Allocate a new task struct. */
	task = kmalloc(sizeof *task);

	if (!task) {
		return NULL;
	}

	/* Set up the virtual address space for the task. */
	if (task_setup_vas(task) < 0) {
		kfree(task);
		return NULL;
	}

	/* Find a free PID for the task in the PID mapping and associate the
	 * task with that PID.
	 */
	for (pid = 1; pid < pid_max; ++pid) {
		if (!tasks[pid]) {
			tasks[pid] = task;
			task->task_pid = pid;
			break;
		}
	}
	/* We are out of PIDs. */
	if (pid == pid_max) {
		page_decref(pa2page(PADDR(task->task_pml4)));
		kfree(task);
		return NULL;
	}

	/* Set up the task. */
	task->task_ppid = ppid;
	task->task_type = TASK_TYPE_USER;
	task->task_status = TASK_RUNNABLE;
	task->task_runs = 0;
	task->task_karma = 0;
	task->task_start_tsc = 0;;
	
#ifndef USE_BIG_KERNEL_LOCK
	spin_init(&task->task_lock, "task_lock",  0); // TODO: Add dynamic name
#endif

	memset(&task->task_frame, 0, sizeof task->task_frame);

	task->task_frame.ds = GDT_UDATA | 3;
	task->task_frame.ss = GDT_UDATA | 3;
	task->task_frame.rsp = USTACK_TOP;
	task->task_frame.cs = GDT_UCODE | 3;
	task->task_frame.rflags = FLAGS_IF; // enable interrupts


	rb_init(&task->task_rb);
	list_init(&task->task_runq);
	list_init(&task->task_mmap);
	list_init(&task->task_children);
	list_init(&task->task_child);
	list_init(&task->task_zombies);
	list_init(&task->task_node);

	#ifdef BONUS_VDSO
	assert(task_setup_vdso(task) == 0);
	#endif

	/* You will set task->task_frame.rip later. */
	cprintf("[PID %5u] New task with PID %u\n",
	        cur_task ? cur_task->task_pid : 0, task->task_pid);

	return task;
}

#ifdef BONUS_ASLR

#define ASLR_CODE_MIN    0x1000000 // lowest address for code
#define ASLR_CODE_SLOTS  (1 << 4)
#define ASLR_STACK_SLOTS (1 << 4)

static void task_relocate_elf(struct elf *elf_hdr, uintptr_t base) {
	
	struct elf_proghdr *prog_hdr = (struct elf_proghdr *)((char *)elf_hdr + elf_hdr->e_phoff);
	struct elf_dyn *dyn;
	struct elf_rela *rela;
	size_t rela_size = 0;

	for (size_t i = 0; i < elf_hdr->e_phnum; i++) 
		if (prog_hdr[i].p_type == ELF_PROG_DYNAMIC) 
			dyn = (struct elf_dyn *)(base + prog_hdr[i].p_va);

	for (; dyn->d_tag != ELF_DYN_NULL; dyn++)
		if (dyn->d_tag == ELF_DYN_RELA) rela = (struct elf_rela *)(base + dyn->d_val);
		else if (dyn->d_tag == ELF_DYN_RELASZ) rela_size = dyn->d_val;

	for (size_t i = 0; i < rela_size / sizeof *rela; i++)
		if (ELF_RELA_TYPE(rela[i].r_info) == ELF_RELOC_X86_64_RELATIVE)
			*(uint64_t *)(base + rela[i].r_offset) = base + rela[i].r_addend;
}
#endif

/* Sets up the initial program binary, stack and processor flags for a user
 * process.
 * This function is ONLY called during kernel initialization, before running
 * the first user-mode environment.
 *
 * This function loads all loadable segments from the ELF binary image into the
 * task's user memory, starting at the appropriate virtual addresses indicated
 * in the ELF program header.
 * At the same time it clears to zero any portions of these segments that are
 * marked in the program header as being mapped but not actually present in the
 * ELF file, i.e., the program's .bss section.
 *
 * Finally, this function maps one page for the program's initial stack.
 */
static int task_load_elf(struct task *task, uint8_t *binary)
{
	/* Hints:
	 * - Load each program segment into virtual memory at the address
	 *   specified in the ELF section header.
	 * - You should only load segments with type ELF_PROG_LOAD.
	 * - Each segment's virtual address can be found in p_va and its
	 *   size in memory can be found in p_memsz.
	 * - The p_filesz bytes from the ELF binary, starting at binary +
	 *   p_offset, should be copied to virtual address p_va.
	 * - Any remaining memory bytes should be zero.
	 * - Use populate_region() and protect_region().
	 * - Check for malicious input.
	 *
	 * Loading the segments is much simpler if you can move data directly
	 * into the virtual addresses stored in the ELF binary.
	 * So in which address space should we be operating during this
	 * function?
	 *
	 * You must also do something with the entry point of the program, to
	 * make sure that the task starts executing there.
	 */

	struct elf *elf_hdr = (struct elf *)binary;
	assert(elf_hdr->e_magic == ELF_MAGIC);

	struct elf_proghdr *prog_hdr = (struct elf_proghdr *)((char *)elf_hdr + elf_hdr->e_phoff);
	
	uintptr_t code_base = 0;
	uintptr_t stack_top = USTACK_TOP;

	#ifdef BONUS_ASLR
		if (elf_hdr->e_type == ELF_TYPE_DYN) code_base = ASLR_CODE_MIN + (read_tsc() % ASLR_CODE_SLOTS) * PAGE_SIZE;
		stack_top = USTACK_TOP - (read_tsc() % ASLR_STACK_SLOTS) * PAGE_SIZE;
	#endif

	for (size_t i = 0; i < elf_hdr->e_phnum; i++, prog_hdr++) {
		if (prog_hdr->p_type != ELF_PROG_LOAD) continue;
		
		uintptr_t va = code_base + prog_hdr->p_va;
		int flags = VM_READ;

		assert(prog_hdr->p_filesz <= prog_hdr->p_memsz);
		assert(va >= prog_hdr->p_va);
		assert(prog_hdr->p_memsz <= USER_LIM - va);

		if (prog_hdr->p_flags & ELF_PROG_FLAG_WRITE) flags |= VM_WRITE;
		if (prog_hdr->p_flags & ELF_PROG_FLAG_EXEC) flags |= VM_EXEC;

		char *name = ".rodata";
		if (flags & VM_EXEC) name = ".text";
		else if (flags & VM_WRITE) name = ".data";

		uintptr_t base = ROUNDDOWN(va, PAGE_SIZE);
		uintptr_t end = ROUNDUP(va + prog_hdr->p_memsz, PAGE_SIZE);

		if (!add_executable_vma(task, name, (void *)base, end - base, flags,binary + prog_hdr->p_offset, prog_hdr->p_filesz, va - base)){
			return -ENOMEM;
		}
	}

	#ifdef BONUS_ASLR
		if (elf_hdr->e_type == ELF_TYPE_DYN) {
			task_relocate_elf(elf_hdr, code_base);
			task->task_frame.rbx = code_base;
		}
		task->task_frame.rsp = stack_top;
		cprintf("[PID %5u][ASLR] image at %p, stack at %p\n", task->task_pid, code_base, stack_top);
	#endif
	task->task_frame.rip = code_base + elf_hdr->e_entry;

	if (!add_anonymous_vma(task, "stack", (void *)(stack_top - PAGE_SIZE), PAGE_SIZE, VM_READ | VM_WRITE)){
		return -ENOMEM;
	}

	return 0;
}

/* Allocates a new task with task_alloc(), loads the named ELF binary using
 * task_load_elf() and sets its task type.
 * If the task is a user task, increment the number of user tasks.
 * This function is ONLY called during kernel initialization, before running
 * the first user-mode task.
 * The new task's parent PID is set to 0.
 */
void task_create(uint8_t *binary, enum task_type type)
{
	struct task *task = task_alloc(0);

	task->task_type = type;
	assert(task_load_elf(task, binary) == 0);
	
	if (type == TASK_TYPE_USER) atomic_inc(&nuser_tasks);
	
	sched_enqueue(task);
}

static void task_dispose_address_space(struct task *task)
{
	free_all_vmas(task);
	unmap_user_pages(task->task_pml4);
	page_decref(pa2page(PADDR(task->task_pml4)));
}

int task_exec(uint8_t *binary)
{
	struct task replacement = {0};
	struct list *node, *next;
	struct vma *vma;
	
	replacement.task_pid = cur_task->task_pid;
	replacement.task_frame.ds = GDT_UDATA | 3;
	replacement.task_frame.ss = GDT_UDATA | 3;
	replacement.task_frame.cs = GDT_UCODE | 3;
	replacement.task_frame.rsp = USTACK_TOP;
	replacement.task_frame.rflags = FLAGS_IF;
	rb_init(&replacement.task_rb);
	list_init(&replacement.task_mmap);

	if (task_setup_vas(&replacement) < 0){
		return -ENOMEM;
	}
	#ifdef BONUS_VDSO
	if (task_setup_vdso(&replacement) < 0) {
		task_dispose_address_space(&replacement);
		return -ENOMEM;
	}
	#endif
	if (task_load_elf(&replacement, binary) < 0) {
		task_dispose_address_space(&replacement);
		return -ENOMEM;
	}

	free_all_vmas(cur_task);
	list_foreach_safe(&replacement.task_mmap, node, next) {
		vma = container_of(node, struct vma, vm_mmap);
		remove_vma(&replacement, vma);
		assert(insert_vma(cur_task, vma) == 0);
	}

	struct page_table *old_pml4 = cur_task->task_pml4;
	cur_task->task_pml4 = replacement.task_pml4;
	cur_task->task_frame = replacement.task_frame;
	load_pml4(PADDR(cur_task->task_pml4));

	// replacement's vma list is empty now, so this for only droping the old page tables
	replacement.task_pml4 = old_pml4;
	task_dispose_address_space(&replacement);

	return 0;
}

/* Free the task and all of the memory that is used by it.
 */
void task_free(struct task *task)
{
	struct task *waiting;

	/* If we are freeing the current task, switch to the kernel_pml4
	 * before freeing the page tables, just in case the page gets re-used.
	 */
	if (task == cur_task) {
		load_pml4(PADDR(kernel_pml4));
	}

	/* Unmap the task from the PID map. */
	tasks[task->task_pid] = NULL;

	fine_spin_lock(&task->task_lock);
	while (!list_is_empty(&task->task_zombies)) 
		task_free(container_of(list_pop(&task->task_zombies), struct task, task_node));

	while (!list_is_empty(&task->task_children)) list_pop(&task->task_children);
	fine_spin_unlock(&task->task_lock);

	list_del(&task->task_child);
	list_del(&task->task_node);

	/* Unmap the user pages. */
	unmap_user_pages(task->task_pml4);
	page_decref(pa2page(PADDR(task->task_pml4)));

	/* Note the task's demise. */
	cprintf("[PID %5u] Freed task with PID %u\n",
	    cur_task ? cur_task->task_pid : task->task_ppid,
 	    task->task_pid);

	free_all_vmas(task);

	if (task->task_type == TASK_TYPE_USER) {
		size_t old = atomic_dec(&nuser_tasks);
		assert(old > 0);
	}

	/* Free the task. */
	kfree(task);
}

/* Frees the task. If the task is the currently running task, then this
 * function runs a new task (and does not return to the caller).
 */
void task_destroy(struct task *task)
{
	bool self = task == cur_task;
	list_del(&task->task_node);
	if (task->task_status == TASK_RUNNABLE) sched_dequeue(task);

	#ifdef BONUS_SLEEP_TIME
	if (task->task_status == TASK_SLEEPING) sched_kick_from_bed(task);
	#endif

	struct task *parent = task->task_ppid ? pid2task(task->task_ppid, 0) : NULL;
	if (parent) fine_spin_lock(&parent->task_lock);

	if (parent && !list_is_empty(&task->task_child)) {
		if (parent->task_status == TASK_NOT_RUNNABLE && (!parent->task_wait || parent->task_wait == task)) {
			if (parent->task_rstatus) {
				// rstatus lives in the parent's address space
				physaddr_t cr3 = read_cr3();
				load_pml4(PADDR(parent->task_pml4));
				*parent->task_rstatus = task->task_exit_status;
				load_pml4(cr3);
			}
			parent->task_frame.rax = task->task_pid;
			sched_enqueue(parent);
			task_free(task);
		} else {
			task->task_status = TASK_DYING;
			if (self) {
				cur_task = NULL;
				load_pml4(PADDR(kernel_pml4));
			}
			list_add(&parent->task_zombies, &task->task_node);
		}
	} else task_free(task);

	if (parent) fine_spin_unlock(&parent->task_lock);

	if (self) {
		cur_task = NULL;
		sched_yield();
	}
}

/*
 * Restores the register values in the trap frame with the iretq or sysretq
 * instruction. This exits the kernel and starts executing the code of some
 * task.
 *
 * This function does not return.
 */
void task_pop_frame(struct int_frame *frame)
{
	struct int_frame f = *frame; // copy the frame to the stack because of lock release

	assert(big_spin_haslock(&kernel_lock));
	big_spin_unlock(&kernel_lock);

	switch (f.int_no) {
#ifdef BONUS_SYSCALL
		case 0x80: sysret64(&f); break;
#endif
		default: iret64(&f); break;
	}

	panic("We should have gone back to userspace!");
}

/* Context switch from the current task to the provided task.
 * Note: if this is the first call to task_run(), cur_task is NULL.
 *
 * This function does not return.
 */
void task_run(struct task *task)
{
	/*
	 * Step 1: If this is a context switch (a new task is running):
	 *     1. Set the current task (if any) back to
	 *        TASK_RUNNABLE if it is TASK_RUNNING (think about
	 *        what other states it can be in),
	 *     2. Set 'cur_task' to the new task,
	 *     3. Set its status to TASK_RUNNING,
	 *     4. Update its 'task_runs' counter,
	 *     5. Use load_pml4() to switch to its address space.
	 * Step 2: Use task_pop_frame() to restore the task's
	 *     registers and drop into user mode in the
	 *     task.
	 *
	 * Hint: This function loads the new task's state from
	 *  e->task_frame.  Go back through the code you wrote above
	 *  and make sure you have set the relevant parts of
	 *  e->task_frame to sensible values.
	 */
	if (cur_task != task) {	
		cur_task = task;
		cur_task->task_status = TASK_RUNNING;
		cur_task->task_runs++;
		load_pml4(PADDR(cur_task->task_pml4));
	}
	assert(cur_task->task_status == TASK_RUNNING);
	task_pop_frame(&cur_task->task_frame);
}

/*
 * Checks that the task is allowed to access the range of memory
 * [va, va + size). If it can, then the function simply returns.
 * If it cannot, the task gets killed and if the task is the current task,
 * this function will not return.
 *
 * Note: this function expects PROT_* flags, not PAGE_*! (see lib.h)
 */
void assert_user_mem(struct task *task, void *va, size_t size, int flags)
{
	uintptr_t fault_va;
	int vma_flags = 0;

	if (flags & PROT_READ)
		vma_flags |= VM_READ;
	if (flags & PROT_WRITE)
		vma_flags |= VM_WRITE;
	if (flags & PROT_EXEC)
		vma_flags |= VM_EXEC;

	if (check_user_vma_range(&fault_va, task, va, size, vma_flags) < 0) {
		cprintf("[PID %5u] Access violation for va %p\n",
			task->task_pid, fault_va);
		task_destroy(task);
	}
}
