#ifndef P101_ERROR_PATH_WALK_PRINTER_H
#define P101_ERROR_PATH_WALK_PRINTER_H

#include "result.h"
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>

bool p101_error_path_walk_status_is_success(int status);
void p101_error_path_walk_print_run_result(const struct p101_env *env, struct p101_error *err, const struct run_result *result);
void p101_error_path_walk_print_status_text(const struct p101_env *env, struct p101_error *err, int status);

#endif    // P101_ERROR_PATH_WALK_PRINTER_H
