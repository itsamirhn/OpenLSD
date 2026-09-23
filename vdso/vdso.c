#include <vdso.h>
#include <time.h>
#include <error.h>

pid_t __vdso_getpid(const struct vdso_data *d) {
	return d->pid;
}

int __vdso_gettimeofday(const struct vdso_data *d, struct timeval *tv, void *tz) {
	if (!tv) return -EINVAL;

	uint64_t delta = read_tsc() - d->tsc_base;
	uint64_t ticks_per_sec = d->tsc_khz * 1000;

	tv->tv_sec = d->epoch_base + (time_t)(delta / ticks_per_sec);
	tv->tv_usec = (suseconds_t)((delta % ticks_per_sec) * 1000 / d->tsc_khz);

	return 0;
}
