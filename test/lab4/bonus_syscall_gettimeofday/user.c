#include <lib.h>

int main(int argc, char **argv) {
	struct timespec a, b;
	volatile int i;

	assert(gettimeofday(&a) == 0);
	assert(a.tv_nsec >= 0 && a.tv_nsec < 1000000000);
	printf("[TIME] gettimeofday is %lld.%09lld\n", (long long)a.tv_sec, (long long)a.tv_nsec);

	for (i = 0; i < 20000000; i++) {}

	assert(gettimeofday(&b) == 0);
	assert(b.tv_nsec >= 0 && b.tv_nsec < 1000000000);
	printf("[TIME] gettimeofday is %lld.%09lld\n after iterations", (long long)b.tv_sec, (long long)b.tv_nsec);

	// Time must not run backwards...
	assert(b.tv_sec > a.tv_sec || (b.tv_sec == a.tv_sec && b.tv_nsec > a.tv_nsec));
	printf("[TIME] gettimeofday is monotonic and advancing\n");

	return 0;
}
