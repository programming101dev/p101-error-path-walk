#ifndef P101_ERROR_PATH_WALK_RESOURCE_H
#define P101_ERROR_PATH_WALK_RESOURCE_H

#include "result.h"
#include <p101_env/env.h>
#include <stdbool.h>
#include <stddef.h>

void p101_error_path_walk_read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary);

#endif    // P101_ERROR_PATH_WALK_RESOURCE_H
