#include <lib.h>

int value = 1;

int main(void) {
	value = 2;
	madvise((void *)ROUNDDOWN((uintptr_t)&value, 4096), 4096, MADV_WILLNEED);
	assert(value == 2);
	return 0;
}
