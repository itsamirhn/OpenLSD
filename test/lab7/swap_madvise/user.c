#include <lib.h>

#define NPAGES 64
#define MSG "Read from swap through syscall.\n"

static char buf[NPAGES * PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));
static char msg[PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

static void fill(char seed)
{
	for (size_t i = 0; i < NPAGES; ++i) memset(buf + i * PAGE_SIZE, seed + i, PAGE_SIZE);
}

static void check(char seed)
{
	for (size_t i = 0; i < NPAGES; ++i){
		for (size_t j = 0; j < PAGE_SIZE; ++j){
			assert(buf[i * PAGE_SIZE + j] == (char)(seed + i));
		}
	}
}

static void swap_out(void *va, size_t len)
{
	assert(madvise(va, len, MADV_SWAP) == 0);
}

int main(int argc, char **argv)
{
	pid_t child;

	// Page in by faulting
	fill(1);
	swap_out(buf, sizeof buf);
	check(1);

	// Page in from a syscall, which restarts before printing anything
	memcpy(msg, MSG, sizeof MSG);
	swap_out(msg, sizeof msg);
	puts(msg, sizeof MSG - 1);

	// Page in with MADV_WILLNEED
	fill(2);
	swap_out(buf, sizeof buf);
	assert(madvise(buf, sizeof buf, MADV_WILLNEED) == 0);
	check(2);

	// The child shares the swap slots with the parent
	fill(3);
	swap_out(buf, sizeof buf);
	if ((child = fork()) == 0) {
		check(3);
		printf("The child sees the shared swap.\n");
		return 0;
	}

	assert(child > 0);
	waitpid(child, NULL, 0);
	check(3);


	printf("MADV_SWAP test successful.\n");
	return 0;
}
