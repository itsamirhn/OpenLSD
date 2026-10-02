OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS {
	.boot PT_LOAD;
	.text PT_LOAD;
	.rodata PT_LOAD;
	.data PT_LOAD;
	.kallsyms PT_LOAD;
}

SECTIONS {
	. = KERNEL_LMA;
	entry = KERNEL_VMA + .;

	.boot ALIGN(4K) : {
		*/kernel/boot.o (.text .data .bss)
		*/kernel/boot_ap.o (.text .data .bss)
		boot_end = .;
	} :.boot

	. += KERNEL_VMA;

	.text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA) ALIGN(4K) {
		*(.text)
		*(.ltext)
	} :.text

	etext = .;

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA) ALIGN(4K) {
		*(.rodata)
		*(.lrodata)
		*(.rodata.str1.*)
		*(.rodata.cst*)
		*(.eh_frame)
	} :.rodata

	.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA) ALIGN(4K) {
		*(.data)
		*(.ldata)
	} :.data

	edata = .;

	.bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA) ALIGN(4K) {
		*(.bss)
		*(.lbss)
		*(COMMON)
	} :.data

	ebss = .;

	.kallsyms ALIGN(4K) : AT(ADDR(.kallsyms) - KERNEL_VMA) ALIGN(4K) {
		*(.kstrtab)
		*(.ksymtab)
	} :.kallsyms

	end = .;
}
