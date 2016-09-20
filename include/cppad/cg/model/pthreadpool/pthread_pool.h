#ifndef CPPADCG_PTHREAD_POOL_H
#define CPPADCG_PTHREAD_POOL_H
/* --------------------------------------------------------------------------
 *  CppADCodeGen: C++ Algorithmic Differentiation with Source Code Generation:
 *    Copyright (C) 2016 Ciengis
 *
 *  CppADCodeGen is distributed under multiple licenses:
 *
 *   - Eclipse Public License Version 1.0 (EPL1), and
 *   - GNU General Public License Version 3 (GPL3).
 *
 *  EPL1 terms and conditions can be found in the file "epl-v10.txt", while
 *  terms and conditions for the GPL3 can be found in the file "gpl3.txt".
 * ----------------------------------------------------------------------------
 * Author: Joao Leal
 */

#ifdef __cplusplus
extern "C" {
#endif

enum group_strategy {SINGLE_ORDERED,
                     MULTI_HALF_TIME};

typedef void (*cppadcg_thpool_function_type)(void*);


void cppadcg_thpool_set_threads(int n);

int cppadcg_thpool_get_threads();

void cppadcg_thpool_set_scheduler_strategy(enum group_strategy s);

enum group_strategy cppadcg_thpool_get_scheduler_strategy();

void cppadcg_thpool_prepare();

void cppadcg_thpool_add_job(cppadcg_thpool_function_type function,
                            void* arg,
                            double* elapsed);

void cppadcg_thpool_add_jobs(cppadcg_thpool_function_type functions[],
                             void* args[],
                             double elapsed[],
                             int order[],
                             int nJobs);

void cppadcg_thpool_wait();

void cppadcg_thpool_update_order(double elapsed[],
                                 int order[],
                                 int nJobs);

void cppadcg_thpool_set_disabled(int disabled);

int cppadcg_thpool_is_disabled();

void cppadcg_thpool_shutdown();

#ifdef __cplusplus
}
#endif

#endif
