
/* System call stubs. */

#include <syscall.h>
#include <lib.h>
#ifdef BONUS_VDSO
#include <vdso.h>
extern uintptr_t vdso_vvar_base;
#endif

extern int64_t do_syscall(uint64_t a1, uint64_t a2,
	uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t num);

static inline int64_t syscall(int num, int check,
	unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4,
	unsigned long a5, unsigned long a6)
{
	int64_t ret;

	/*
	 * In OpenLSD, system calls follow the same conventions as used by Linux. In
	 * other words, this is the normal System V AMD64 ABI calling conventions
	 * with some small modifications to deal with system call specifics.
	 * Concretely, our convention is as follows:
	 *  - The system call number is passed in RAX
	 *  - The six parameters of a system call are passed in
	 *      RDI, RSI, RDX, RCX, R8, and R9, in that order.
	 *  - The response of a system call is in RAX.
	 *  - For the SYSCALL bonus (non-interrupt route), we use R10 instead of RCX.
	 *
	 * The special rules for RAX and RCX/R10 are implemented in Assembly in
	 * do_syscall. Therefore, we can call do_syscall like a normal function with
	 * the syscall number as seventh parameter (on the stack).
	 */
	ret = do_syscall(a1, a2, a3, a4, a5, a6, num);

	if(check && ret < 0) {
		panic("syscall %d returned %d (> 0)", num, ret);
	}

	return ret;
}

void puts(const char *s, size_t len)
{
	syscall(SYS_cputs, 0, (uintptr_t)s, len, 0, 0, 0, 0);
}

int getc(void)
{
	return syscall(SYS_cgetc, 0, 0, 0, 0, 0, 0, 0);
}

int kill(pid_t pid)
{
	return syscall(SYS_kill, 1, pid, 0, 0, 0, 0, 0);
}

void exit(int exitcode)
{
	syscall(SYS_exit, 0, exitcode, 0, 0, 0, 0, 0);
}

pid_t getpid(void)
{
	#ifdef BONUS_VDSO
	static pid_t (*vgetpid)(const struct vdso_data *data);
	if (!vgetpid) {
		const struct vdso_data *vdso_data = (const struct vdso_data *)vdso_vvar_base;
		vgetpid = vdso_sym((void *)vdso_data->vdso_base, "__vdso_getpid");
	}
	if (vgetpid) return vgetpid((const struct vdso_data *)vdso_vvar_base);
	#endif
	 return syscall(SYS_getpid, 0, 0, 0, 0, 0, 0, 0);
}

int clock_gettime(clockid_t clock, struct timespec *ts)
{
	#ifdef BONUS_VDSO
	static int (*vclock_gettime)(const struct vdso_data *data, clockid_t clock, struct timespec *ts);
	if (!vclock_gettime) {
		const struct vdso_data *vdso_data = (const struct vdso_data *)vdso_vvar_base;
		vclock_gettime = vdso_sym((void *)vdso_data->vdso_base, "__vdso_clock_gettime");
	}
	if (vclock_gettime) return vclock_gettime((const struct vdso_data *)vdso_vvar_base, clock, ts);
	#endif
	return syscall(SYS_clock_gettime, 0, clock, (uintptr_t)ts, 0, 0, 0, 0);
}

int gettimeofday(struct timeval *tv) {
	#ifdef BONUS_VDSO
	if (!tv) return -EINVAL;
	struct timespec ts;
	int ret = clock_gettime(CLOCK_REALTIME, &ts);
	if (ret < 0) return ret;
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
	#endif
	return syscall(SYS_gettimeofday, 0, (uintptr_t)tv, 0, 0, 0, 0, 0);
}

time_t time(time_t *t) {
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) < 0) return -1;
	if (t) *t = ts.tv_sec;
	return ts.tv_sec;
}

int clock_nanosleep(clockid_t clock, int flags, const struct timespec *req, struct timespec *rem) {
	#ifdef BONUS_SLEEP_TIME
	return syscall(SYS_clock_nanosleep, 0, clock, flags, (uintptr_t)req, 0, 0, 0);
	#else
	return -ENOSYS;
	#endif
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
	return clock_nanosleep(CLOCK_MONOTONIC, 0, req, rem);
}

int usleep(unsigned int usec) {
	struct timespec ts = { .tv_sec = USEC_TO_SEC(usec), .tv_nsec = USEC_TO_NSEC((usec % USEC_PER_SEC)) };
	return nanosleep(&ts, NULL);
}

unsigned int sleep(unsigned int seconds) {
	struct timespec ts = { .tv_sec = seconds, .tv_nsec = 0 };
	nanosleep(&ts, NULL);
	return 0;
}

int mquery(struct vma_info *info, void *addr)
{
	return syscall(SYS_mquery, 0, (uint64_t)info, (uint64_t)addr, 0, 0, 0, 0);
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, uintptr_t offset)
{
	return (void *)syscall(SYS_mmap, 0, (uint64_t)addr, len, prot, flags, fd, offset);
}

void munmap(void *addr, size_t len)
{
	syscall(SYS_munmap, 0, (uint64_t)addr, len, 0, 0, 0, 0);
}

int mprotect(void *addr, size_t len, int prot)
{
	return syscall(SYS_mprotect, 0, (uint64_t)addr, len, prot, 0, 0, 0);
}

int madvise(void *addr, size_t len, int advice)
{
	return syscall(SYS_madvise, 0, (uint64_t)addr, len, advice, 0, 0, 0);
}

void sched_yield(void)
{
	syscall(SYS_yield, 0, 0, 0, 0, 0, 0, 0);
}

pid_t wait(int *rstatus)
{
	return syscall(SYS_wait, 0, (uint64_t)rstatus, 0, 0, 0, 0, 0);
}

pid_t waitpid(pid_t pid, int *rstatus, int opts)
{
	return syscall(SYS_waitpid, 0, (uint64_t)pid, (uint64_t)rstatus, opts, 0, 0, 0);
}

pid_t fork(void)
{
	return syscall(SYS_fork, 0, 0, 0, 0, 0, 0, 0);
}

int exec(char *binary_name)
{
	return syscall(SYS_exec, 0, (uintptr_t)binary_name, 0, 0, 0, 0, 0);
}

unsigned int getcpuid(void)
{
	return syscall(SYS_getcpuid, 0, 0, 0, 0, 0, 0, 0);
}

int sched_setaffinity(pid_t pid, unsigned cpusetsize, cpu_set_t *mask)
{
	return syscall(SYS_sched_setaffinity, 0, pid, cpusetsize, (uint64_t)mask, 0, 0, 0);
}

int sched_getaffinity(pid_t pid, unsigned cpusetsize, cpu_set_t *mask)
{
	return syscall(SYS_sched_getaffinity, 0, pid, cpusetsize, (uint64_t)mask, 0, 0, 0);
}
