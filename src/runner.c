#include "runner.h"
#include "constants.h"
#include "errors.h"
#include "paths.h"
#include "printer.h"
#include "resource.h"
#include "result.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_stdio.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#ifdef P101_ERROR_PATH_WALK_TESTING
extern void __gcov_dump(void);
#endif

enum
{
    FAULT_GROUP_LIMIT = 128
};

struct fault_group
{
    char   name[NAME_LEN];
    size_t runs;
    size_t findings;
};

static int    run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result);
static int    run_p101_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result);
static void   update_fault_group(const struct p101_env *env, struct p101_error *err, struct fault_group groups[FAULT_GROUP_LIMIT], size_t *group_count, const struct run_result *result);
static void   print_fault_groups(const struct p101_env *env, struct p101_error *err, const struct fault_group groups[FAULT_GROUP_LIMIT], size_t group_count);
static size_t resource_finding_count(const struct run_result *result);
static bool   resource_summary_unavailable(const struct run_result *result);
static bool   observe_status_is_acceptable(int status);
static void   clear_fault_environment(const struct p101_env *env, struct p101_error *err);
static void   reset_run_environment(const struct p101_env *env, struct p101_error *err);
static void   flush_standard_streams(const struct p101_env *env, struct p101_error *err);

int p101_error_path_walk_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct run_result  result;
    unsigned int       index;
    size_t             runs;
    size_t             resource_findings;
    struct fault_group groups[FAULT_GROUP_LIMIT];
    size_t             group_count;
    bool               trouble;

    P101_TRACE_SCOPE(env);
    runs              = 0;
    resource_findings = 0;
    group_count       = 0;
    p101_memset(env, groups, 0, sizeof(groups));
    trouble = false;

    if(run_one_case(env, err, args, 0, &result) != EXIT_SUCCESS)
    {
        trouble = true;
        goto done;
    }

    runs++;
    p101_error_path_walk_print_run_result(env, err, &result);

    if((int)result.observe_ok == 0 || resource_summary_unavailable(&result) || (!p101_error_path_walk_status_is_success(result.status) && resource_finding_count(&result) == 0U))
    {
        trouble = true;
    }

    resource_findings += resource_finding_count(&result);

    for(index = 1; index <= args->max_failures && p101_error_has_no_error(err); index++)
    {
        if(run_one_case(env, err, args, index, &result) != EXIT_SUCCESS)
        {
            trouble = true;
            goto done;
        }

        runs++;
        p101_error_path_walk_print_run_result(env, err, &result);

        if((int)result.observe_ok == 0 || resource_summary_unavailable(&result))
        {
            trouble = true;
        }

        if((int)result.fault_hit == 0)
        {
            p101_printf(env, err, "p101-error-path-walk: exhausted after %u fault-capable call%s.\n", index - 1U, (index - 1U) == 1U ? "" : "s");
            break;
        }

        resource_findings += resource_finding_count(&result);
        update_fault_group(env, err, groups, &group_count, &result);
    }

    p101_printf(env, err, "p101-error-path-walk: %zu run%s, %zu resource finding%s.\n", runs, runs == 1 ? "" : "s", resource_findings, resource_findings == 1 ? "" : "s");
    print_fault_groups(env, err, groups, group_count);

done:
    reset_run_environment(env, err);

    if(p101_error_has_error(err) || trouble)
    {
        return EXIT_TROUBLE;
    }

    if(resource_findings > 0)
    {
        return EXIT_FINDINGS;
    }

    return EXIT_SUCCESS;
}

static void update_fault_group(const struct p101_env *env, struct p101_error *err, struct fault_group groups[FAULT_GROUP_LIMIT], size_t *group_count, const struct run_result *result)
{
    const char *name;
    size_t      findings;
    size_t      index;

    if(result->fault_index == 0 || !result->fault_hit)
    {
        goto done;
    }

    name     = (result->fault_name[0] == '\0') ? "?" : result->fault_name;
    findings = resource_finding_count(result);
    index    = *group_count;

    for(size_t i = 0; i < *group_count; i++)
    {
        if(p101_strcmp(env, groups[i].name, name) == 0)
        {
            index = i;
            break;
        }
    }

    if(index == *group_count)
    {
        if(*group_count >= FAULT_GROUP_LIMIT)
        {
            goto done;
        }

        p101_strncpy(env, groups[index].name, name, sizeof(groups[index].name) - 1U);
        groups[index].name[sizeof(groups[index].name) - 1U] = '\0';
        (*group_count)++;
    }

    groups[index].runs++;
    groups[index].findings += findings;

done:
    (void)err;
}

static void print_fault_groups(const struct p101_env *env, struct p101_error *err, const struct fault_group groups[FAULT_GROUP_LIMIT], size_t group_count)
{
    if(group_count == 0U)
    {
        goto done;
    }

    p101_fputs(env, err, "p101-error-path-walk: grouped by faulted wrapper:\n", stdout);

    for(size_t i = 0; i < group_count; i++)
    {
        p101_printf(env, err, "  %s: %zu run%s, %zu resource finding%s\n", groups[i].name, groups[i].runs, groups[i].runs == 1U ? "" : "s", groups[i].findings, groups[i].findings == 1U ? "" : "s");
    }

done:
    return;
}

static size_t resource_finding_count(const struct run_result *result)
{
    if(!result->resources.parsed)
    {
        return 0U;
    }

    return result->resources.fd_leaks + result->resources.allocation_leaks + result->resources.bad_releases + result->resources.exec_inheritances + result->resources.generic_resource_leaks + result->resources.generic_bad_releases;
}

static bool resource_summary_unavailable(const struct run_result *result)
{
    return (!result->resource_log_present || !result->resources.parsed || !result->resources.log_complete) != 0;
}

static int run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    char fault_value[FAULT_LEN];

    P101_TRACE_SCOPE(env);
    p101_memset(env, result, 0, sizeof(*result));
    result->fault_index = fault_index;
    p101_error_path_walk_make_log_paths(env, err, args, fault_index, result);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    clear_fault_environment(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_setenv(env, err, CHILD_FAULT_LOG_ENV, result->fault_log_path, 1);

    if(args->fault_name != NULL)
    {
        p101_setenv(env, err, CHILD_FAULT_NAME_ENV, args->fault_name, 1);
    }

    p101_setenv(env, err, CHILD_FAULT_MODE_ENV, args->fault_mode, 1);
    {
        char amount_value[FAULT_LEN];
        char repeat_value[FAULT_LEN];

        p101_snprintf(env, err, amount_value, sizeof(amount_value), "%u", args->fault_amount);
        p101_snprintf(env, err, repeat_value, sizeof(repeat_value), "%u", args->fault_repeat);
        p101_setenv(env, err, CHILD_FAULT_AMOUNT_ENV, amount_value, 1);
        p101_setenv(env, err, CHILD_FAULT_REPEAT_ENV, repeat_value, 1);
    }

    if(args->fault_errno_str != NULL)
    {
        char errno_value[FAULT_LEN];

        p101_snprintf(env, err, errno_value, sizeof(errno_value), "%d", args->fault_errno);
        p101_setenv(env, err, CHILD_FAULT_ERRNO_ENV, errno_value, 1);
    }

    if(fault_index > 0)
    {
        p101_snprintf(env, err, fault_value, sizeof(fault_value), "%u", fault_index);
        p101_setenv(env, err, CHILD_FAULT_CALL_ENV, fault_value, 1);
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result->status     = run_p101_observe(env, err, args, result);
    result->observe_ok = observe_status_is_acceptable(result->status);
    result->fault_hit  = p101_error_path_walk_read_fault_hit(env, err, result->fault_log_path, result->fault_name);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result->resource_log_present = p101_error_path_walk_file_exists(env, result->resource_log_path);

    if(result->resource_log_present && p101_error_path_walk_file_exists(env, result->resource_json_path))
    {
        p101_error_path_walk_read_resource_json(env, err, result->resource_json_path, &result->resources);
    }

done:
    clear_fault_environment(env, err);

    if(p101_error_has_error(err))
    {
        return EXIT_TROUBLE;
    }

    return EXIT_SUCCESS;
}

static int run_p101_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result)
{
    char  *tool_argv[MAX_TOOL_ARGS];
    char   observe_path[PATH_LEN];
    char   observe_dir[PATH_LEN];
    char   tracker_path[PATH_LEN];
    char   concurrency_path[PATH_LEN];
    char   trace_path[PATH_LEN];
    char   report_path[PATH_LEN];
    char   output_option[]      = "-o";
    char   tracker_option[]     = "-r";
    char   concurrency_option[] = "-d";
    char   trace_option[]       = "-t";
    char   report_option[]      = "-p";
    char   separator[]          = "--";
    size_t index;
    size_t command_index;
    int    status;
    pid_t  pid;

    P101_TRACE_SCOPE(env);
    status = 0;
    p101_strncpy(env, observe_path, args->p101_observe, sizeof(observe_path) - 1U);
    observe_path[sizeof(observe_path) - 1U] = '\0';
    p101_strncpy(env, observe_dir, result->observe_dir, sizeof(observe_dir) - 1U);
    observe_dir[sizeof(observe_dir) - 1U] = '\0';
    p101_strncpy(env, tracker_path, args->resource_tracker, sizeof(tracker_path) - 1U);
    tracker_path[sizeof(tracker_path) - 1U] = '\0';
    p101_strncpy(env, concurrency_path, args->p101_sync_check, sizeof(concurrency_path) - 1U);
    concurrency_path[sizeof(concurrency_path) - 1U] = '\0';
    p101_strncpy(env, trace_path, args->p101_trace, sizeof(trace_path) - 1U);
    trace_path[sizeof(trace_path) - 1U] = '\0';
    p101_strncpy(env, report_path, args->p101_report, sizeof(report_path) - 1U);
    report_path[sizeof(report_path) - 1U] = '\0';

    index              = 0;
    tool_argv[index++] = observe_path;
    tool_argv[index++] = output_option;
    tool_argv[index++] = observe_dir;
    tool_argv[index++] = tracker_option;
    tool_argv[index++] = tracker_path;
    tool_argv[index++] = concurrency_option;
    tool_argv[index++] = concurrency_path;
    tool_argv[index++] = trace_option;
    tool_argv[index++] = trace_path;
    tool_argv[index++] = report_option;
    tool_argv[index++] = report_path;
    tool_argv[index++] = separator;

    command_index = 0U;
    while(args->command_argv[command_index] != NULL && index < MAX_TOOL_ARGS - 1U)
    {
        tool_argv[index++] = args->command_argv[command_index++];
    }
    tool_argv[index] = NULL;

    if(args->command_argv[command_index] != NULL)
    {
        P101_ERROR_RAISE_USER(err, "The command has too many arguments for p101-error-path-walk.", ERR_USAGE);
        goto done;
    }

    flush_standard_streams(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    pid = p101_fork(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(pid == 0)
    {
        if(p101_freopen(env, err, result->observe_stdout_path, "w", stdout) == NULL || p101_freopen(env, err, result->observe_stderr_path, "w", stderr) == NULL)
        {
            p101_fprintf(env, err, stderr, "p101-error-path-walk: observe setup failed: %s\n", p101_error_get_message(err));
#ifdef P101_ERROR_PATH_WALK_TESTING
            __gcov_dump();
#endif
            p101_posix_exit_immediately(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, tool_argv[0], tool_argv);
        p101_fprintf(env, err, stderr, "p101-error-path-walk: exec failed for %s: %s\n", args->p101_observe, p101_error_get_message(err));
#ifdef P101_ERROR_PATH_WALK_TESTING
        __gcov_dump();
#endif
        p101_posix_exit_immediately(env, EXEC_FAILURE);
    }

    p101_waitpid(env, err, pid, &status, 0);

done:
    return status;
}

static bool observe_status_is_acceptable(int status)
{
    bool acceptable;

    acceptable = false;
    if(WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS || WEXITSTATUS(status) == EXIT_FINDINGS))
    {
        acceptable = true;
    }

    return acceptable;
}

static void clear_fault_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE_SCOPE(env);
    p101_unsetenv(env, err, FAULT_CALL_ENV);
    p101_unsetenv(env, err, FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, FAULT_LOG_ENV);
    p101_unsetenv(env, err, FAULT_NAME_ENV);
    p101_unsetenv(env, err, FAULT_MODE_ENV);
    p101_unsetenv(env, err, FAULT_AMOUNT_ENV);
    p101_unsetenv(env, err, FAULT_REPEAT_ENV);
    p101_unsetenv(env, err, CALL_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ARGS_ENV);
    p101_unsetenv(env, err, CALL_LOG_RESULT_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_CALL_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_LOG_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_NAME_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_MODE_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_AMOUNT_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_REPEAT_ENV);
}

static void reset_run_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE_SCOPE(env);
    clear_fault_environment(env, err);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
}

static void flush_standard_streams(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE_SCOPE(env);
    p101_fflush(env, err, stdout);

    if(p101_error_has_no_error(err))
    {
        p101_fflush(env, err, stderr);
    }
}

#ifdef P101_ERROR_PATH_WALK_TESTING
int p101_error_path_walk_test_run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    return run_one_case(env, err, args, fault_index, result);
}

int p101_error_path_walk_test_run_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result)
{
    return run_p101_observe(env, err, args, result);
}

bool p101_error_path_walk_test_observe_status(int status)
{
    return observe_status_is_acceptable(status);
}

size_t p101_error_path_walk_test_resource_finding_count(const struct run_result *result)
{
    return resource_finding_count(result);
}

bool p101_error_path_walk_test_resource_summary_unavailable(const struct run_result *result)
{
    return resource_summary_unavailable(result);
}

void p101_error_path_walk_test_exercise_fault_groups(const struct p101_env *env, struct p101_error *err)
{
    struct fault_group groups[FAULT_GROUP_LIMIT];
    struct run_result  result;
    size_t             group_count;

    p101_memset(env, groups, 0, sizeof(groups));
    p101_memset(env, &result, 0, sizeof(result));
    group_count = 0U;
    print_fault_groups(env, err, groups, group_count);
    update_fault_group(env, err, groups, &group_count, &result);
    result.fault_index = 1U;
    update_fault_group(env, err, groups, &group_count, &result);
    result.fault_hit          = true;
    result.resources.parsed   = true;
    result.resources.fd_leaks = 1U;
    update_fault_group(env, err, groups, &group_count, &result);
    update_fault_group(env, err, groups, &group_count, &result);

    for(size_t index = group_count; index < FAULT_GROUP_LIMIT; index++)
    {
        p101_snprintf(env, err, result.fault_name, sizeof(result.fault_name), "call-%zu", index);
        update_fault_group(env, err, groups, &group_count, &result);
    }
    p101_strncpy(env, result.fault_name, "overflow", sizeof(result.fault_name));
    update_fault_group(env, err, groups, &group_count, &result);
    print_fault_groups(env, err, groups, group_count);
}
#endif
