#ifndef P101_ERROR_PATH_WALK_TEST_HOOKS_H
#define P101_ERROR_PATH_WALK_TEST_HOOKS_H

#include "arguments.h"
#include "result.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stddef.h>

int    p101_error_path_walk_test_run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result);
int    p101_error_path_walk_test_run_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result);
bool   p101_error_path_walk_test_observe_status(int status);
size_t p101_error_path_walk_test_analysis_finding_count(const struct run_result *result);
bool   p101_error_path_walk_test_analysis_summary_unavailable(const struct run_result *result);
void   p101_error_path_walk_test_exercise_fault_groups(const struct p101_env *env, struct p101_error *err);
void   p101_error_path_walk_test_force_error_create_failure(bool force);
void   p101_error_path_walk_test_handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option);

#endif
