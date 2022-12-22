/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>

#define OVERHEAD_BENCH_PARAMS(n) { .name = n }
#define RUNS 5000

enum overheads {
    CALL_OVERHEAD,
    REPLY_RECV_OVERHEAD,
    SEND_OVERHEAD,
    RECV_OVERHEAD,
    CALL_10_OVERHEAD,
    REPLY_RECV_10_OVERHEAD,
    /******/
    NUM_OVERHEAD_BENCHMARKS
};

typedef enum dir {
    /* client ---> server */
    DIR_TO,
    /* server --> client */
    DIR_FROM
} dir_t;

typedef enum {
    IPC_CALL_FUNC = 0,
    IPC_CALL_FUNC2 = 1,
    IPC_CALL_10_FUNC = 2,
    IPC_CALL_10_FUNC2 = 3,
    IPC_REPLYRECV_FUNC2 = 4,
    IPC_REPLYRECV_FUNC = 5,
    IPC_REPLYRECV_10_FUNC2 = 6,
    IPC_REPLYRECV_10_FUNC = 7,
    IPC_SEND_FUNC = 8,
    IPC_RECV_FUNC = 9
} helper_func_id_t;

typedef seL4_Word(*helper_func_t)(int argc, char *argv[]);

typedef struct benchmark_params {
    /* name of the function we are benchmarking */
    const char *name;
    /* direction of the ipc */
    dir_t direction;
    /* functions for client and server to run */
    helper_func_id_t server_fn, client_fn;
    /* should client and server run in the same vspace? */
    bool same_vspace;
    /* prio for client and server to run at */
    uint8_t server_prio, client_prio;
    /* length of ipc to send */
    uint8_t length;
    /* id of overhead calculation for this function */
    enum overheads overhead_id;
    /* if CONFIG_KERNEL_MCS, should the server be passive? */
    bool passive;
} benchmark_params_t;

struct overhead_benchmark_params {
    const char *name;
};

/* array of benchmarks to run */
/* one way IPC benchmarks - varying size, direction and priority.*/
static const benchmark_params_t benchmark_params[] = {
    /* Call fastpath between client and server in different address spaces */
    {
        .name        = "seL4_Call",
        .direction   = DIR_TO,
        .client_fn   = IPC_CALL_FUNC2,
        .server_fn   = IPC_REPLYRECV_FUNC2,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = CALL_OVERHEAD,
        .passive = true,
    },
    /* ReplyRecv fastpath between server and client in different address spaces */
    {
        .name        = "seL4_ReplyRecv",
        .direction   = DIR_FROM,
        .client_fn   = IPC_CALL_FUNC,
        .server_fn   = IPC_REPLYRECV_FUNC,
        .same_vspace = false,
        .client_prio = seL4_MaxPrio - 1,
        .server_prio = seL4_MaxPrio - 1,
        .length = 0,
        .overhead_id = REPLY_RECV_OVERHEAD,
        .passive = true,
    },
};

static const struct overhead_benchmark_params overhead_benchmark_params[] = {
    [CALL_OVERHEAD]          = {"call"},
    [REPLY_RECV_OVERHEAD]    = {"reply recv"},
    [SEND_OVERHEAD]          = {"send"},
    [RECV_OVERHEAD]          = {"recv"},
    [CALL_10_OVERHEAD]       = {"call"},
    [REPLY_RECV_10_OVERHEAD] = {"reply recv"},
};

typedef struct ipc_results {
    /* Raw results from benchmarking. These get checked for sanity */
    ccnt_t overhead_benchmarks[NUM_OVERHEAD_BENCHMARKS][RUNS];
    ccnt_t benchmarks[ARRAY_SIZE(benchmark_params)][RUNS];
} ipc_results_t;

static inline bool results_stable(ccnt_t *array, size_t size)
{
    for (size_t i = 1; i < size; i++) {
        if (array[i] != array[i - 1]) {
            return false;
        }
    }

    return true;
}
