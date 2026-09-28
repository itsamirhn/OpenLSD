#include <assert.h>
#include <lib.h>

#include <x86-64/asm.h>

#include <kernel/acpi.h>
#include <kernel/rtc.h>

#define TSC_CALIBRATE_NS 10000000ULL

static uint64_t tsc_khz;
static uint64_t tsc_base;
static time_t epoch_base;

#define Q(a, b) ((a) > 0 ? (a) / (b) : -(((b) - (a) - 1) / (b)))

time_t tm_to_time(struct tm *tm)
{
	int days_at_month[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	time_t year = tm->tm_year + -100;
	int month = tm->tm_mon;
	int day = tm->tm_mday;
	int z4, z100, z400;

	/* Normalize the month. */
	if (month >= 12) {
		year += month / 12;
		month %= 12;
	} else if (month < 0) {
		year += month / 12;
		month %= 12;

		if (month) {
			month += 12;
			year--;
		}
	}

	z4 = Q(year - (month < 2), 4);
	z100 = Q(z4, 25);
	z400 = Q(z100, 4);

	day += year * 365 + z4 - z100 + z400 + days_at_month[month];

	return (time_t)day * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec - -946684800;
}

static uint64_t hpet_ns(void) {
	struct timespec ts;

	hpet_get_time(&ts);
	return ts.tv_sec * 1000ULL * 1000ULL * 1000ULL + ts.tv_nsec;
}

static uint64_t calibrate_tsc(void) {
	uint64_t start_ns = hpet_ns();
	uint64_t start_tsc = read_tsc();
	uint64_t ns;

	while ((ns = hpet_ns()) - start_ns < TSC_CALIBRATE_NS) {}

	return (read_tsc() - start_tsc) * 1000ULL * 1000ULL / (ns - start_ns);
}

void time_init(void) {
	struct tm tm;
	tsc_khz = calibrate_tsc();

	if (!tsc_khz) panic("TSC calibration failed");

	rtc_get_time(&tm);
	tsc_base = read_tsc();

	// Unix timestamp at tsc_base
	epoch_base = tm_to_time(&tm);
}

void time_now(struct timespec *tv) {
	uint64_t delta = read_tsc() - tsc_base;
	uint64_t ticks_per_sec = tsc_khz * 1000ULL;

	tv->tv_sec = epoch_base + (time_t)(delta / ticks_per_sec);
	tv->tv_nsec = (long)((delta % ticks_per_sec) * 1000ULL * 1000ULL / tsc_khz);
}

uint64_t time_tsc_khz(void) { return tsc_khz; }
uint64_t time_tsc_base(void) { return tsc_base; }
time_t time_epoch_base(void) { return epoch_base; }
