#include <lib.h>

#define SLACK_NS MSEC_TO_NSEC(100) // Allow 100ms of slack for sleep tests ...

static uint64_t now_ns(void) {
	struct timespec ts;
	assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
	return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static void check(const char *what, uint64_t start, uint64_t want_ns) {
	uint64_t took = now_ns() - start;

	printf("[TEST] %s slept %llu ns\n", what, took);
	assert(want_ns <= took);
	assert(took < want_ns + SLACK_NS);
	printf("[TEST] %s ok\n", what);
}

int main(void) {
	uint64_t start = now_ns();
	assert(sleep(1) == 0);
	check("sleep(1)", start, NSEC_PER_SEC);

	start = now_ns();
	assert(usleep(MSEC_TO_USEC(250)) == 0);
	check("usleep(250 ms)", start, MSEC_TO_NSEC(250));

	struct timespec ts = { .tv_sec = 0, .tv_nsec = MSEC_TO_NSEC(300) };
	start = now_ns();
	assert(nanosleep(&ts, NULL) == 0);
	check("nanosleep(300 ms)", start, MSEC_TO_NSEC(300));

	start = now_ns();
	assert(usleep(0) == 0);
	check("usleep(0)", start, 0);

	return 0;
}
