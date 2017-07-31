/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#ifndef __SELBENCH_ARCH_SMP_H
#define __SELBENCH_ARCH_SMP_H

#include <benchmark.h>

#define CACHE_LN_SZ 32
#define OVERHEAD_MEAS_RUNS 16

#define READ_CYCLE_COUNTER(_r) do {                                 \
    SEL4BENCH_READ_CCNT(_r);                                        \
} while(0)
#define RESET_CYCLE_COUNTER do {                                    \
    sel4bench_private_write_pmcr(SEL4BENCH_ARMV7A_PMCR_RESET_CCNT | \
        SEL4BENCH_ARMV7A_PMCR_ENABLE);                              \
} while(0)
#define OVERHEAD_FIXUP(_c, _o) ((_c) - (_o))

static inline void
smp_benchmark_ping(seL4_CPtr ep)
{
    seL4_CallWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL, NULL);
}

static inline void
smp_benchmark_pong(seL4_CPtr ep)
{
    seL4_ReplyRecvWithMRs(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, NULL, NULL, NULL, NULL);
}

static inline ccnt_t
smp_benchmark_check_overhead(void)
{
    uint32_t ccnt_overhead;
    uint32_t begin, end, total = 0;

    sel4bench_init();
    for (int i = 0; i < OVERHEAD_MEAS_RUNS; i++) {
        COMPILER_MEMORY_FENCE();
        READ_CYCLE_COUNTER(begin);

        READ_CYCLE_COUNTER(end);
        COMPILER_MEMORY_FENCE();
        total += (end - begin);
    }
    ccnt_overhead = total / OVERHEAD_MEAS_RUNS;
    total = 0;
    for (int i = 0; i < OVERHEAD_MEAS_RUNS; i++) {
        COMPILER_MEMORY_FENCE();
        RESET_CYCLE_COUNTER;

        READ_CYCLE_COUNTER(end);
        COMPILER_MEMORY_FENCE();
        total += (end - ccnt_overhead);
    }
    sel4bench_destroy();

    return total / OVERHEAD_MEAS_RUNS;
}

#endif /* __SELBENCH_ARCH_SMP_H */