#include <types.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

#include <x86-64/asm.h>
#include <kernel/time.h>

#define RTC_INDEX 0x70
#define RTC_DATA  0x71

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS   0x04

static uint64_t tsc_khz;
static uint64_t tsc_base;
static time_t epoch_base;

static uint8_t rtc_read(uint8_t reg) {
	outb(RTC_INDEX, reg);
	return inb(RTC_DATA);
}

static uint64_t calibrate_tsc(void) {
	// Align to a seconds boundary so we measure a whole second
	uint8_t mark = rtc_read(RTC_SECONDS);
	while (rtc_read(RTC_SECONDS) == mark) {}

	uint64_t start = read_tsc();

	mark = rtc_read(RTC_SECONDS);
	while (rtc_read(RTC_SECONDS) == mark) {}

	uint64_t end = read_tsc();

	return (end - start) / 1000;
}

void time_init(void) {
	tsc_khz = calibrate_tsc();

	if (!tsc_khz) panic("TSC calibration failed");

	tsc_base = read_tsc();

	// Unix timestamp at tsc_base; 0 = 1970-01-01 00:00:00 UTC
	epoch_base = 0;
}

void time_now(struct timeval *tv) {
	uint64_t delta = read_tsc() - tsc_base;
	uint64_t ticks_per_sec = tsc_khz * 1000;

	tv->tv_sec = epoch_base + (time_t)(delta / ticks_per_sec);
	tv->tv_usec = (suseconds_t)((delta % ticks_per_sec) * 1000 / tsc_khz);
}
