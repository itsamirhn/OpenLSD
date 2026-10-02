#include <lib.h>

#define WAIT_MS 1000 // how long we are blocked while the rivals run
#define RACE_MS 500 // how long we compete with the rivals after waking up
#define TOLERANCE_PERCENT 6

static uint64_t now_ns(void) {
	struct timespec ts;
	assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
	return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static int busy_loop(uint64_t count_from, uint64_t until) {
	int loops = 0;
	for (uint64_t now = now_ns(); now < until; now = now_ns()) loops += (now >= count_from);
	return loops;
}

static void expect_fair_share(const char *who, int fair_percent, int loops, int total) {
	int percent = loops * 100 / total;
	printf("[TEST] %s got %d%% of the CPU after the wake up\n", who, percent);
	assert(fair_percent - TOLERANCE_PERCENT <= percent); // not starved
	assert(percent <= fair_percent + TOLERANCE_PERCENT); // not favored
}

int main(void) {
	uint64_t wake_time = now_ns() + MSEC_TO_NSEC(WAIT_MS);
	uint64_t end_time = wake_time + MSEC_TO_NSEC(RACE_MS);
	int my_loops, rival1_loops, rival2_loops;

	// Two rivals busy loop the whole time, but only count their loops after wake_time
	pid_t rival1 = fork();
	if (rival1 == 0) return busy_loop(wake_time, end_time);
	pid_t rival2 = fork();
	if (rival2 == 0) return busy_loop(wake_time, end_time);

	// We use alarm to get behind in karma vs rivals
	pid_t alarm = fork();
	if (alarm == 0) {
		while (now_ns() < wake_time) {}
		return 0;
	}
	assert(waitpid(alarm, NULL, 0) == alarm);

	// Now compete with the rivals
	my_loops = busy_loop(wake_time, end_time);
	assert(waitpid(rival1, &rival1_loops, 0) == rival1);
	assert(waitpid(rival2, &rival2_loops, 0) == rival2);

	int total = my_loops + rival1_loops + rival2_loops;
	int fair_percent = 100 / 3; // 3 tasks, so each should get ~33% of the CPU
	expect_fair_share("me", fair_percent, my_loops, total);
	expect_fair_share("rival1", fair_percent, rival1_loops, total);
	expect_fair_share("rival2", fair_percent, rival2_loops, total);
	printf("[TEST] woken task does not starve others\n");

	return 0;
}
