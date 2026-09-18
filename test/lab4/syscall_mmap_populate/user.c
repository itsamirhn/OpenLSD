#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
	char *addr = (void *)0x1000000;
	char *ret;

	for (int i = 0; i < 6; i++)
	{
		addr += PAGE_SIZE * i;

		// Sometimes populate, sometimes not - test that populate does not
		// trigger demand paging page fault upon memset
		uint64_t flags = MAP_ANONYMOUS | MAP_PRIVATE;

		// Trigger this three times
		if(i % 2) flags |= MAP_POPULATE;

		ret = mmap(addr, PAGE_SIZE, PROT_READ | PROT_WRITE, flags, -1, 0);
		assert(ret != MAP_FAILED);

		memset(addr, 0, PAGE_SIZE);
	}

	print_vmas();

	return 0;
}
