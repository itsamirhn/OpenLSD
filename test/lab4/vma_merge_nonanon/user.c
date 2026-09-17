/*
 * Test: Non-anonymous VMAs should merge when backing memory is continuous
 *
 * This test verifies that two adjacent non-anonymous VMAs
 * merge when they have the same permissions and their backing memory is
 * exactly consecutive.
 *
 * The kernel test creates two non-anonymous VMAs at consecutive
 * addresses with continuous backing memory. This user program verifies
 * the backing data is still properly accessible across the merged VMA,
 * and prints the VMAs to verify the two have been merged into one.
 */

#include <lib.h>


void check_memory(char *p, size_t len, char val) 
{
	for (size_t i = 0; i < len; i++) {
		if (p[i] != val)
			panic("Found invalid byte in non-anon vma area!");
	}
}

int main(int argc, char **argv)
{
	check_memory((char *)0x1000000, 0x100, 0x00);
	check_memory((char *)0x1000100, (PAGE_SIZE * 2) - 0x100, 0xAA);

	/* Print VMAs to verify all three remain separate */
	print_vmas();
	
	return 0;
}

