#include <lib.h>

int main(int argc, char **argv) {

	struct timeval *tv = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
	    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	assert(tv != MAP_FAILED);
	assert(gettimeofday(tv) == 0);
	assert(tv->tv_sec != 0);

	return 0;
}
