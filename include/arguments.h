#ifndef ERROR_PATH_WALK_ARGUMENTS_H
#define ERROR_PATH_WALK_ARGUMENTS_H

#include <stdbool.h>

enum
{
    PATH_LEN = 1024,
    NAME_LEN = 128
};

struct arguments
{
    const char  *max_failures_str;
    const char  *fault_errno_str;
    const char  *fault_name;
    const char  *fault_mode;
    const char  *fault_amount_str;
    const char  *fault_repeat_str;
    const char  *log_prefix;
    const char  *p101_observe;
    const char  *resource_tracker;
    const char  *p101_trace;
    const char  *p101_report;
    char *const *command_argv;
    unsigned int max_failures;
    int          fault_errno;
    unsigned int fault_amount;
    unsigned int fault_repeat;
    bool         verbose;
    bool         stop_at_exhaustion;
};

#endif    // ERROR_PATH_WALK_ARGUMENTS_H
