/*
 * Test: Non-anonymous VMAs should not merge with anonymous VMAs
 *
 * This test verifies that a non-anonymous VMA does not merge
 * with adjacent anonymous VMAs, even when all three have the same
 * permissions.
 *
 * The kernel test creates two anonymous VMAs with a
 * non-anonymous VMA in between them, all with
 * VM_READ | VM_WRITE. This user program verifies the backing data of the
 * non-anonymous VMA is still accessible, and prints the VMAs to verify
 * all three remain separate.
 */

#include <lib.h>

int main(int argc, char **argv)
{
	char *p = (char *)0x1001000;
	for (size_t i = 0; i < PAGE_SIZE; i++) {
		if (p[i] != (char)0xAA)
			panic("Found invalid byte in non-anon vma area!");
	}

	/* Print VMAs to verify all three remain separate (not merged) */
	print_vmas();
	
	return 0;
}

