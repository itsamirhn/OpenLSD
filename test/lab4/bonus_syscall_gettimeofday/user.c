#include <lib.h>

int main(int argc, char **argv) {
	struct timeval a, b;
	volatile int i;

	assert(gettimeofday(&a, NULL) == 0);
	assert(a.tv_usec >= 0 && a.tv_usec < 1000000);
	printf("[TIME] gettimeofday is %lld.%06lld\n", (long long)a.tv_sec, (long long)a.tv_usec);

	for (i = 0; i < 20000000; i++) {}

	assert(gettimeofday(&b, NULL) == 0);
	assert(b.tv_usec >= 0 && b.tv_usec < 1000000);
	printf("[TIME] gettimeofday is %lld.%06lld\n after iterations", (long long)b.tv_sec, (long long)b.tv_usec);

	// Time must not run backwards...
	assert(b.tv_sec > a.tv_sec || (b.tv_sec == a.tv_sec && b.tv_usec > a.tv_usec));
	printf("[TIME] gettimeofday is monotonic and advancing\n");

	return 0;
}
