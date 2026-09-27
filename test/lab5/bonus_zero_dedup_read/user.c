#include <lib.h>

#define VM_RAM_SIZE (32 * 1024 * 1024)
#define SIZE (4 * VM_RAM_SIZE)

int main(void) {
	char *p = mmap((void *)0x10000000, SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	// Only fits if every page maps the same zero page
	for (size_t i = 0; i < SIZE; i += 4096) assert(p[i] == 0);

	return 0;
}
