#include <vdso.h>
#include <lib.h>
#include <error.h>

pid_t __vdso_getpid(const struct vdso_data *d) {
	return d->pid;
}

int __vdso_clock_gettime(const struct vdso_data *d, clockid_t clock, struct timespec *ts) {
	if (!ts) return -EINVAL;
	if (clock != CLOCK_REALTIME && clock != CLOCK_MONOTONIC) return -EINVAL;

	uint64_t delta = read_tsc() - d->tsc_base;
	uint64_t ticks_per_sec = d->tsc_khz * 1000;

	ts->tv_sec = (time_t)(delta / ticks_per_sec);
	ts->tv_nsec = (long)((delta % ticks_per_sec) * 1000000 / d->tsc_khz);
	if (clock == CLOCK_REALTIME) ts->tv_sec += d->epoch_base;

	return 0;
}
