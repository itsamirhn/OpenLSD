/* Try to map NULL. */
#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
	uint64_t overflow = 0xffffc00000000000;

	assert(mmap((void *)USER_LIM - PAGE_SIZE, overflow + 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
	     MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0) == MAP_FAILED);

	print_vmas();

	return 0;
}
