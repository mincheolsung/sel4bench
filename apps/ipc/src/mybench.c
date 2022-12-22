/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
/* This is very much a work in progress IPC benchmarking set. Goal is
   to eventually use this to replace the rest of the random benchmarking
   happening in this app with just what we need */

#include <autoconf.h>
#include <sel4benchipc/gen_config.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4bench/sel4bench.h>
#include <sel4utils/process.h>
#include <string.h>
#include <utils/util.h>
#include <vka/vka.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>
#include <utils/attribute.h>

#include <benchmark.h>
#include <ipc.h>

/* arch/ipc.h requires these defines */
#define NOPS ""

#include <arch/ipc.h>

#define NUM_ARGS 3
/* Ensure that enough warmups are performed to prevent the FPU from
 * being restored. */
#ifdef CONFIG_FPU_MAX_RESTORES_SINCE_SWITCH

#else
#error "or,, reach out here?"
#endif

typedef struct helper_thread {
    sel4utils_process_t process;
    seL4_CPtr ep;
    seL4_CPtr result_ep;
    char *argv[NUM_ARGS];
    char argv_strings[NUM_ARGS][WORD_STRING_SIZE];
} helper_thread_t;

static void timing_init(void)
{
    sel4bench_init();
#ifdef CONFIG_GENERIC_COUNTER
    event_id_t event = GENERIC_EVENTS[CONFIG_GENERIC_COUNTER_ID];
    sel4bench_set_count_event(0, event);
    sel4bench_reset_counters();
    sel4bench_start_counters(GENERIC_COUNTER_MASK);
#endif
#ifdef CONFIG_PLATFORM_COUNTER
    sel4bench_set_count_event(0, CONFIG_PLATFORM_COUNTER_CONSTANT);
    sel4bench_reset_counters();
    sel4bench_start_counters(GENERIC_COUNTER_MASK);
#endif
}

void timing_destroy(void)
{
#ifdef CONFIG_GENERIC_COUNTER
    sel4bench_stop_counters(GENERIC_COUNTER_MASK);
    sel4bench_destroy();
#endif
}

static inline void dummy_cache_func(void) {}

#ifdef CONFIG_CLEAN_L1_ICACHE
#define CACHE_FUNC() do {                           \
    seL4_BenchmarkFlushL1Caches(seL4_ARM_CacheI);   \
} while (0)

#elif CONFIG_CLEAN_L1_DCACHE
#define CACHE_FUNC() do {                           \
    seL4_BenchmarkFlushL1Caches(seL4_ARM_CacheD);   \
} while (0)

#elif CONFIG_CLEAN_L1_CACHE
#define CACHE_FUNC() do {                           \
    seL4_BenchmarkFlushL1Caches(seL4_ARM_CacheID);  \
} while (0)

#elif CONFIG_DIRTY_L1_DCACHE
#define L1_CACHE_LINE_SIZE BIT(CONFIG_L1_CACHE_LINE_SIZE_BITS)
#define POLLUTE_ARRARY_SIZE CONFIG_L1_DCACHE_SIZE/L1_CACHE_LINE_SIZE/sizeof(int)
#define POLLUTE_RUNS 5
#define CACHE_FUNC() do {                                       \
    ALIGN(L1_CACHE_LINE_SIZE) volatile                          \
    int pollute_array[POLLUTE_ARRARY_SIZE][L1_CACHE_LINE_SIZE]; \
    for (int i = 0; i < POLLUTE_RUNS; i++) {                    \
        for (int j = 0; j < L1_CACHE_LINE_SIZE; j++) {          \
            for (int k = 0; k < POLLUTE_ARRARY_SIZE; k++) {     \
                pollute_array[k][j]++;                          \
            }                                                   \
        }                                                       \
    }                                                           \
} while (0)

#else
#define CACHE_FUNC dummy_cache_func
#endif

seL4_Word client(int argc, char *argv[]);
seL4_Word server(int argc, char *argv[]);

static helper_func_t bench_funcs[] = {
    client,
    server,
};

#define ITER 10000000
seL4_Word client(int argc, char *argv[]) {
    uint32_t i,j;
    //ccnt_t cnt = 0;
    ccnt_t start UNUSED, end UNUSED;
    seL4_CPtr ep = atoi(argv[0]);
    seL4_CPtr result_ep = atoi(argv[1]);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Call(ep, tag);
    for (j = 0; j < 10; j++) {
        COMPILER_MEMORY_FENCE();
        READ_COUNTER_BEFORE(start);
        for (i = 0; i < ITER; i++) {
            //seL4_SetMR(0, cnt + 0xBEEF00000000);
            //tag = seL4_MessageInfo_new(0, 0, 0, 1);
            seL4_Call(ep, tag);
            //cnt++;
        }
        READ_COUNTER_AFTER(end);
        COMPILER_MEMORY_FENCE();
        send_result(result_ep, (end-start)/ITER);
    }
    return 0;
}

seL4_Word server(int argc, char *argv[]) {
    ccnt_t message;
    ccnt_t cnt = 0;
    ccnt_t start UNUSED, end UNUSED;
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_CPtr ep = atoi(argv[0]);
    seL4_CPtr result_ep = atoi(argv[1]);
    seL4_CPtr reply = atoi(argv[2]);
    seL4_Recv(ep, &reply);
    while(1) {
        seL4_ReplyRecv(ep, tag, &reply);
        //message = seL4_GetMR(0);
        //if (message != (0xBEEF00000000 + cnt)) {
            //cnt += 0xDEAD000000000000;
            //send_result(result_ep, cnt);
            //return 0;
        //}
        //cnt++;
    }
    return 0;
}

void run_bench(env_t *env, cspacepath_t result_ep_path, seL4_CPtr ep,
               helper_thread_t *client, helper_thread_t *server)
{

    timing_init();

    /* start processes */
    int error = benchmark_spawn_process(&server->process, &env->slab_vka, &env->vspace, NUM_ARGS,
                                        server->argv, 1);
    ZF_LOGF_IF(error, "Failed to spawn server\n");

    error = benchmark_spawn_process(&client->process, &env->slab_vka, &env->vspace, NUM_ARGS, client->argv, 1);
    ZF_LOGF_IF(error, "Failed to spawn client\n");

    /* get results */
    uint64_t cycle[10];
    for (int i = 0; i < 10; i++) {
        cycle[i] = get_result(result_ep_path.capPtr);
        //if (cycle & 0xDEAD000000000000) {
        //    ZF_LOGE("%d reply: %lX, failed at: %lu\n", i, cycle, cycle-0xDEAD000000000000);
        //} else {
            //ZF_LOGE("%lu", cycle);
        //}
    }
    for (int i = 0; i < 10; i++) {
        ZF_LOGE("%lu", cycle[i]);
    }
    /* clean up - clean server first in case it is sharing the client's cspace and vspace */
    seL4_TCB_Suspend(client->process.thread.tcb.cptr);
    seL4_TCB_Suspend(server->process.thread.tcb.cptr);

    timing_destroy();
}

static env_t *env;

void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) init_env(void)
{
    static size_t object_freq[seL4_ObjectTypeCount] = {
        [seL4_TCBObject] = 4,
        [seL4_EndpointObject] = 2,
    };

    env = benchmark_get_env(
              sel4runtime_argc(),
              sel4runtime_argv(),
              sizeof(ipc_results_t),
              object_freq
          );
}

static bool set_affinity = false;
int main(int argc, char **argv)
{
    vka_object_t ep, result_ep;
    cspacepath_t ep_path, result_ep_path;

    ipc_results_t *results = (ipc_results_t *) env->results;

    /* allocate benchmark endpoint - the IPC's that we benchmark
       will be sent over this ep */
    if (vka_alloc_endpoint(&env->slab_vka, &ep) != 0) {
        ZF_LOGF("Failed to allocate endpoint");
    }
    vka_cspace_make_path(&env->slab_vka, ep.cptr, &ep_path);

    /* allocate result ep - the IPC threads will send their timestamps
       to this ep */
    if (vka_alloc_endpoint(&env->slab_vka, &result_ep) != 0) {
        ZF_LOGF("Failed to allocate endpoint");
    }
    vka_cspace_make_path(&env->slab_vka, result_ep.cptr, &result_ep_path);

    helper_thread_t client, server_process;

    benchmark_shallow_clone_process(env, &client.process, seL4_MinPrio, 0, "client");
    benchmark_shallow_clone_process(env, &server_process.process, seL4_MinPrio, 0, "server process");

    client.ep = sel4utils_copy_path_to_process(&client.process, ep_path);
    client.result_ep = sel4utils_copy_path_to_process(&client.process, result_ep_path);

    server_process.ep = sel4utils_copy_path_to_process(&server_process.process, ep_path);
    server_process.result_ep = sel4utils_copy_path_to_process(&server_process.process, result_ep_path);

    sel4utils_create_word_args(client.argv_strings, client.argv, NUM_ARGS, client.ep, client.result_ep, 0);
    sel4utils_create_word_args(server_process.argv_strings, server_process.argv, NUM_ARGS,
                               server_process.ep, server_process.result_ep, SEL4UTILS_REPLY_SLOT);

    /* run the benchmark */
    seL4_CPtr auth = simple_get_tcb(&env->simple);

    /* set up client for benchmark */
    int error = seL4_TCB_SetPriority(client.process.thread.tcb.cptr, auth, seL4_MaxPrio - 1);
    ZF_LOGF_IF(error, "Failed to set client prio");
    client.process.entry_point = bench_funcs[0];

    error = seL4_TCB_SetPriority(server_process.process.thread.tcb.cptr, auth, seL4_MaxPrio - 1);
    assert(error == seL4_NoError);
    server_process.process.entry_point = bench_funcs[1];

#if 1
	/* set affinity */
    set_affinity = true;
	error = seL4_TCB_SetAffinity(server_process.process.thread.tcb.cptr, 2);
    ZF_LOGF_IF(error, "Failed to set affinity of server\n");
	error = seL4_TCB_SetAffinity(client.process.thread.tcb.cptr, 4);
    ZF_LOGF_IF(error, "Failed to set affinity of client\n");
#endif

    ZF_LOGE("run bench, iter: %d, affinity %sset", ITER, set_affinity ? "":"not ");
    run_bench(env, result_ep_path, ep_path.capPtr, &client, &server_process);

    return 0;
}
