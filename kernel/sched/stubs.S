
.section .text

#include <x86-64/gdt.h>
#include <x86-64/idt.h>
#include <x86-64/memory.h>

#include <cpu.h>


.macro ISR_NOERRCODE int_no
.global isr\int_no
isr\int_no:
	pushq $0
	pushq $\int_no

	jmp isr_common_stub
.endm

.macro ISR_ERRCODE int_no
.global isr\int_no
isr\int_no:
	pushq $\int_no

	jmp isr_common_stub
.endm


ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_ERRCODE   30
ISR_NOERRCODE 32
ISR_NOERRCODE 127
ISR_NOERRCODE 128

isr_common_stub:
	/* Save the register state on the stack. */
	/* Recover the segments. */
	pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rbx
	pushq %rbp
	pushq %rsi
	pushq %rdi
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	xorq %rax, %rax
	movq %ds, %rax
	pushq %rax /* ds */

	movw $GDT_KDATA, %ax
	movw %ax, %ds
	movw %ax, %es

	/* Jump to the generic interrupt handler. */
	movq %rsp, %rdi
	jmp int_handler

.global syscall64
syscall64:
#ifdef BONUS_SYSCALL
	swapgs
	/* Store the userspace stack pointer in the per-cpu struct. */
	movq %rsp, %gs:CPU_TSS_RSP3
	movq %r12, %gs:CPU_TSS_RSP2

	/* Load the kernel stack pointer from the per-cpu struct. */
	movq %gs:CPU_TSS_RSP0, %rsp

	/* Store the register state into the per-cpu struct. */ 
	movq %gs:CPU_TASK, %r12
	movq $GDT_UDATA | 3, 0(%r12)

	movq %r15, 8(%r12)
	movq %r14, 16(%r12)
	movq %r13, 24(%r12)
	movq %gs:CPU_TSS_RSP2, %r13

	movq %r13, 32(%r12)
	movq %r11, 40(%r12)
	movq %r10, 48(%r12)
	movq %r9, 56(%r12)
	movq %r8, 64(%r12)
	movq %rdi, 72(%r12)
	movq %rsi, 80(%r12)
	movq %rbp, 88(%r12)
	movq %rbx, 96(%r12)
	movq %rdx, 104(%r12)
	movq %rcx, 112(%r12)
	movq %rax, 120(%r12)
	movq $0x80, 128(%r12)
	movq $0, 136(%r12)
	movq %rcx, 144(%r12)
	movq $GDT_UCODE | 3, 152(%r12)
	movq %r11, 160(%r12)
	movq %gs:CPU_TSS_RSP3, %rax
	movq %rax, 168(%r12)
	movq $GDT_UDATA | 3, 176(%r12)

	/* Jump to the system call handler. This is implemented as a normal function
	 * that expects the syscall number as its seventh argument, so we push the
	 * syscall number to the stack again. Also restore the RCX register and add
	 * a dummy return address.
	 */
	movq %r10, %rcx
	movq 120(%r12), %rax
	pushq %rax
	pushq $0
	jmp syscall_handler
#endif

.global iret64
iret64:
	/* Restore the register state. */
	movq %rdi, %rsp
#ifdef BONUS_L1TF_MDS
	movw $GDT_KDATA, %ax
	verw %ax
#endif

	popq %rax // ds
	movw %ax, %ds

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rdi
	popq %rsi
	popq %rbp
	popq %rbx
	popq %rdx
	popq %rcx
	popq %rax

	/* Return from the interrupt. */
	addq $16, %rsp
	iretq

.global sysret64
sysret64:
#ifdef BONUS_SYSCALL
#ifdef BONUS_L1TF_MDS
	movw $GDT_KDATA, %ax
	verw %ax
#endif

	/* Recover the register state. */
	movq 8(%rdi), %r15
	movq 16(%rdi), %r14
	movq 24(%rdi), %r13
	movq 32(%rdi), %r12
	movq 40(%rdi), %r11
	movq 48(%rdi), %r10
	movq 56(%rdi), %r9
	movq 64(%rdi), %r8
	movq 80(%rdi), %rsi
	movq 88(%rdi), %rbp
	movq 96(%rdi), %rbx
	movq 104(%rdi), %rdx
	movq 120(%rdi), %rax
	movq 144(%rdi), %rcx
	movq 168(%rdi), %rsp
	movq 72(%rdi), %rdi
	swapgs
	/* Return from the system call. */
	sysretq
#endif
