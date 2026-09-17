/*
 * Test: Adjacent non-anonymous VMAs should not merge when permissions
 * differ or backing memory is not continuous
 *
 * This test verifies that three adjacent non-anonymous VMAs
 * do not merge when either their permissions differ or their backing
 * memory is not continuous.
 *
 * The kernel test creates three non-anonymous VMAs at consecutive
 * addresses with differing permissions and non-continuous backing.
 * This user program verifies the backing data of each VMA is still
 * accessible, and prints the VMAs to verify all three remain separate.
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
	check_memory((char *)0x1000000, PAGE_SIZE, 0xAA);
	check_memory((char *)0x1001000, PAGE_SIZE, 0xBB);
	check_memory((char *)0x1002000, PAGE_SIZE - 0x10, 0xCC);
	check_memory((char *)0x1003000, 0x100, 0x00);
	check_memory((char *)0x1003100, PAGE_SIZE - 0x100, 0xDD);
	check_memory((char *)0x1004000, PAGE_SIZE, 0xEE);
	check_memory((char *)0x1006000, PAGE_SIZE, 0xFF);

	/* Print VMAs to verify all three remain separate (not merged) */
	print_vmas();
	
	return 0;
}

