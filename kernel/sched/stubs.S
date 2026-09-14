
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

/* LAB 3: your code here. */

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
	/* LAB 3: your bonus code here. */
	/* Store the userspace stack pointer in the per-cpu struct. */

	/* Load the kernel stack pointer from the per-cpu struct. */

	/* Store the register state into the per-cpu struct. */

	/* Jump to the system call handler. This is implemented as a normal function
	 * that expects the syscall number as its seventh argument, so we push the
	 * syscall number to the stack again. Also restore the RCX register and add
	 * a dummy return address.
	 */
	movq %r10, %rcx
	pushq %rax
	pushq $0
	jmp syscall_handler

.global iret64
iret64:
	/* Restore the register state. */
	movq %rdi, %rsp

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
	/* LAB 3: your bonus code here. */
	/* Recover the register state. */
	/* Return from the system call. */
	sysretq
