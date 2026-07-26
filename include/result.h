#ifndef P101_ERROR_PATH_WALK_RESULT_H
#define P101_ERROR_PATH_WALK_RESULT_H

#include "arguments.h"
#include <stdbool.h>
#include <stddef.h>

struct resource_summary
{
    size_t records;
    size_t fd_leaks;
    size_t allocation_leaks;
    size_t bad_releases;
    bool   parsed;
};

struct run_result
{
    unsigned int            fault_index;
    int                     status;
    bool                    fault_hit;
    bool                    resource_log_present;
    bool                    observe_ok;
    char                    fault_name[NAME_LEN];
    char                    observe_dir[PATH_LEN];
    char                    resource_log_path[PATH_LEN];
    char                    call_log_path[PATH_LEN];
    char                    fault_log_path[PATH_LEN];
    char                    resource_json_path[PATH_LEN];
    char                    report_path[PATH_LEN];
    char                    observe_stdout_path[PATH_LEN];
    char                    observe_stderr_path[PATH_LEN];
    struct resource_summary resources;
};

#endif    // P101_ERROR_PATH_WALK_RESULT_H
