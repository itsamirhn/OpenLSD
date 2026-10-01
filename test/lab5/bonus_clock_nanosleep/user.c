#include <lib.h>

#define SLACK_NS MSEC_TO_NSEC(100) // Allow 100ms of slack for sleep tests ...

static uint64_t now_ns(clockid_t clock) {
	struct timespec ts;
	assert(clock_gettime(clock, &ts) == 0);
	return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static int sleep_until(clockid_t clock, int flags, uint64_t ns) {
	struct timespec ts = { .tv_sec = ns / NSEC_PER_SEC, .tv_nsec = ns % NSEC_PER_SEC };
	return clock_nanosleep(clock, flags, &ts, NULL);
}

static int sleep_raw(clockid_t clock, time_t sec, long nsec) {
	struct timespec ts = { .tv_sec = sec, .tv_nsec = nsec };
	return clock_nanosleep(clock, 0, &ts, NULL);
}

static void check_relative(clockid_t clock, const char *name) {
	uint64_t start = now_ns(CLOCK_MONOTONIC);
	assert(sleep_until(clock, 0, MSEC_TO_NSEC(200)) == 0);
	uint64_t took = now_ns(CLOCK_MONOTONIC) - start;

	printf("[TEST] relative %s slept %llu ns\n", name, took);
	assert(MSEC_TO_NSEC(200) <= took);
	assert(took < MSEC_TO_NSEC(200) + SLACK_NS);
	printf("[TEST] relative %s ok\n", name);
}

static void check_absolute(clockid_t clock, const char *name) {
	uint64_t deadline = now_ns(clock) + MSEC_TO_NSEC(200);
	assert(sleep_until(clock, TIMER_ABSTIME, deadline) == 0);
	uint64_t now = now_ns(clock);

	printf("[TEST] absolute %s woke %llu ns after the deadline\n", name, now - deadline);
	assert(deadline <= now);
	assert(now < deadline + SLACK_NS);
	printf("[TEST] absolute %s ok\n", name);
}

int main(void) {
	check_relative(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
	check_relative(CLOCK_REALTIME, "CLOCK_REALTIME");
	check_absolute(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
	check_absolute(CLOCK_REALTIME, "CLOCK_REALTIME");

	// A deadline that has already passed returns right away
	uint64_t start = now_ns(CLOCK_MONOTONIC);
	assert(sleep_until(CLOCK_MONOTONIC, TIMER_ABSTIME, 0) == 0);
	assert(sleep_until(CLOCK_REALTIME, TIMER_ABSTIME, now_ns(CLOCK_REALTIME) - MSEC_TO_NSEC(1)) == 0);
	assert(now_ns(CLOCK_MONOTONIC) - start < SLACK_NS);
	printf("[TEST] absolute deadline in the past ok\n");

	return 0;
}
