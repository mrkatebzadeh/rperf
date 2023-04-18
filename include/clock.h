/*
 * Copyright (c) 2005 Mellanox Technologies Ltd.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id$
 *
 * Author: Michael S. Tsirkin <mst@mellanox.co.il>
 */

#ifndef GET_CLOCK_H
#define GET_CLOCK_H

#include <assert.h>
#include <stdint.h> /* for uint64_t */
#include <stdio.h>
#include <time.h> /* for struct timespec */

#define ENABLE_STATIC_TICKS_PER_NS 1
#define RDTSC_TYPICAL_TICKS_PER_NS 2.2

extern double g_ticks_per_ns;

#if defined(__X86_64__) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
// assembly code to read the TSC
static inline uint64_t RDTSC() {
    unsigned int hi, lo;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#elif defined(__aarch64__) || defined(_M_ARM64)
// assembly code to read the TSC
static inline uint64_t RDTSC() {
    unsigned int lo;
    __asm__ __volatile__ ("mrs %0, cntvct_el0" : "=r" (lo));
    return lo;
}
#endif

static const int NANO_SECONDS_IN_SEC = 1000000000;
// returns a static buffer of struct timespec with the time difference of
// ts1 and ts2 ts1 is assumed to be greater than ts2
static struct timespec *timespec_diff(
    struct timespec *ts1, struct timespec *ts2) {
    static struct timespec ts;
    ts.tv_sec  = ts1->tv_sec - ts2->tv_sec;
    ts.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
    if (ts.tv_nsec < 0) {
        ts.tv_sec--;
        ts.tv_nsec += NANO_SECONDS_IN_SEC;
    }
    return &ts;
}

static struct timespec timespec_duration(
    struct timespec *ts1, struct timespec *ts2) {
    struct timespec ts;
    ts.tv_sec  = ts1->tv_sec - ts2->tv_sec;
    ts.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
    if (ts.tv_nsec < 0) {
        ts.tv_sec--;
        ts.tv_nsec += NANO_SECONDS_IN_SEC;
    }
    return ts;
}
static void calibrate_ticks() {
    struct timespec begin_ts, end_ts;
    printf("Start RDTSC calibration: patience is a virtue\n");
    clock_gettime(CLOCK_MONOTONIC, &begin_ts);
    uint64_t begin = RDTSC();
    // do something CPU intensive
    for (volatile unsigned long long i = 0; i < 1000000000ULL; ++i)
        ;
    uint64_t end = RDTSC();
    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    struct timespec *tmp_ts = timespec_diff(&end_ts, &begin_ts);
    uint64_t ns_elapsed =
        (uint64_t)(tmp_ts->tv_sec * 1000000000LL + tmp_ts->tv_nsec);
    g_ticks_per_ns = (double)(end - begin) / (double)ns_elapsed;
    printf("RDTSC calibration is done (ticks_per_ns: %.2f)\n", g_ticks_per_ns);
}

// Call once (it is not thread safe) before using RDTSC, has side effect of
// binding process to CPU1
static inline void init_rdtsc(uint8_t auto_calibration, double ticks_per_ns) {
    if (auto_calibration > 0)
        calibrate_ticks();
    else {
        assert(ticks_per_ns > 0);
        g_ticks_per_ns = ticks_per_ns;
    }
}

static inline void get_timespec(struct timespec *ts, uint64_t nsecs) {
    ts->tv_sec  = nsecs / NANO_SECONDS_IN_SEC;
    ts->tv_nsec = nsecs % NANO_SECONDS_IN_SEC;
}

// ts will be filled with time converted from TSC reading
static inline void get_rdtsc_timespec(struct timespec *ts) {
    get_timespec(ts, (uint64_t)(RDTSC() / g_ticks_per_ns));
}

static inline double time_elapsed_in_us(struct timespec start) {
    struct timespec now, *diff;
    get_rdtsc_timespec(&now);
    diff = timespec_diff(&now, &start);
    return diff->tv_sec * 1000000 + diff->tv_nsec / 1000;
}

static inline double time_elapsed_in_ms(struct timespec start) {
    struct timespec now, *diff;
    get_rdtsc_timespec(&now);
    diff = timespec_diff(&now, &start);
    return diff->tv_sec * 1000 + diff->tv_nsec / 1000000;
}

static inline double time_elapsed_in_sec(struct timespec start) {
    struct timespec now, *diff;
    get_rdtsc_timespec(&now);
    diff = timespec_diff(&now, &start);
    return diff->tv_sec + diff->tv_nsec / NANO_SECONDS_IN_SEC;
}

static inline double time_diff_in_ns(struct timespec end, struct timespec start) {
    struct timespec diff;
    diff = timespec_duration(&end, &start);
    return diff.tv_sec * 1000000000 + diff.tv_nsec;
}

extern double get_cpu_mhz(int);

#endif
