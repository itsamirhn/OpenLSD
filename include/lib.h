
#pragma once

#include <types.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <error.h>
#include <assert.h>
#include <task.h>
#include <syscall.h>
#include <paging.h>

#include <x86-64/memory.h>

typedef long long time_t;

struct tm {
	int tm_sec, tm_min, tm_hour;
	int tm_mday, tm_mon, tm_year;
	int tm_wday, tm_yday;
	int tm_isdst;
};

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

struct timeval {
	time_t tv_sec;
	long tv_usec;
};

typedef int clockid_t;

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

// clock_nanosleep flag for absolute time
#define TIMER_ABSTIME   1

#define MSEC_PER_SEC (1000ULL)
#define USEC_PER_MSEC (1000ULL)
#define NSEC_PER_USEC (1000ULL)
#define USEC_PER_SEC (USEC_PER_MSEC * MSEC_PER_SEC)
#define NSEC_PER_SEC (NSEC_PER_USEC * USEC_PER_SEC)
#define NSEC_PER_MSEC (USEC_PER_MSEC * NSEC_PER_USEC) 
#define USEC_TO_SEC(usec) ((usec) / (USEC_PER_MSEC * MSEC_PER_SEC))
#define USEC_TO_NSEC(usec) ((usec) * NSEC_PER_USEC)
#define MSEC_TO_USEC(msec) ((msec) * USEC_PER_MSEC)
#define MSEC_TO_NSEC(msec) ((msec) * NSEC_PER_MSEC)
#define SEC_TO_NSEC(sec) ((sec) * NSEC_PER_SEC)

enum {
	VMA_FREE = 0,
	VMA_ANONYMOUS,
	VMA_EXECUTABLE,
};

enum {
	VM_UNMAPPED = 0,
	VM_4K_PAGE,
	VM_2M_PAGE,
};

struct vma_info {
	char vm_name[64];
	void *vm_base, *vm_end;
	int vm_prot, vm_type, vm_mapped;
};

#define USED(x) (void)(x)

/* main user program */
void umain(int argc, char **argv);

/* libmain.c or entry.S */
extern const char *binary_name;

/* readline.c */
char *readline(const char *buf);

/* syscall.c */
void puts(const char *string, size_t len);
int getc(void);
pid_t getpid(void);
int kill(pid_t);
void exit(int);
int gettimeofday(struct timeval *tv);
int clock_gettime(clockid_t clock, struct timespec *ts);
time_t time(time_t *t);
int clock_nanosleep(clockid_t clock, int flags, const struct timespec *req, struct timespec *rem);
int nanosleep(const struct timespec *req, struct timespec *rem);
int usleep(unsigned int usec);
unsigned int sleep(unsigned int seconds);

#ifdef BONUS_VDSO
void *vdso_sym(void *base, const char *name);
#endif


int mquery(struct vma_info *info, void *addr);
void *mmap(void *addr, size_t len, int prot, int flags, int fd,
	uintptr_t offset);
void munmap(void *addr, size_t len);
int mprotect(void *addr, size_t len, int prot);
int madvise(void *addr, size_t len, int advise);

/* File open modes */
#define O_RDONLY    0x0000      /* open for reading only */
#define O_WRONLY    0x0001      /* open for writing only */
#define O_RDWR      0x0002      /* open for reading and writing */
#define O_ACCMODE   0x0003      /* mask for above modes */

#define O_CREAT     0x0100      /* create if nonexistent */
#define O_TRUNC     0x0200      /* truncate to zero length */
#define O_EXCL      0x0400      /* error if already exists */
#define O_MKDIR     0x0800      /* create directory, not regular file */

#define PROT_NONE     0
#define PROT_READ     (1 << 0)
#define PROT_WRITE    (1 << 1)
#define PROT_EXEC     (1 << 2)

#define MAP_PRIVATE   (1 << 0)
#define MAP_ANONYMOUS (1 << 1)
#define MAP_POPULATE  (1 << 4)
#define MAP_FIXED     (1 << 5)
#define MAP_FAILED    (void *)(0xffffffffffffffffull)

#define MADV_WILLNEED 1
#define MADV_DONTNEED 2


void sched_yield(void);
pid_t wait(int *rstatus);
pid_t waitpid(pid_t pid, int *rstatus, int opts);
pid_t fork(void);
int exec(char *binary_name);

/* time.c */
time_t tm_to_time(struct tm *tm);
void time_init(void);
void time_now(struct timespec *tv);
void time_monotonic(struct timespec *tv);
uint64_t time_tsc_khz(void);
uint64_t time_tsc_base(void);
time_t time_epoch_base(void);

/* vma.c */
void print_vmas(void);
