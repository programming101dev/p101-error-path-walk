#ifndef P101_ERROR_PATH_WALK_PATHS_H
#define P101_ERROR_PATH_WALK_PATHS_H

#include "arguments.h"
#include "result.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>

void p101_error_path_walk_make_log_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result);
bool p101_error_path_walk_file_exists(const struct p101_env *env, const char *path);
bool p101_error_path_walk_read_fault_hit(const struct p101_env *env, struct p101_error *err, const char *path, char name[NAME_LEN]);

#endif    // P101_ERROR_PATH_WALK_PATHS_H
