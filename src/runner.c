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

static int  run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result);
static int  run_p101_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result);
static bool observe_status_is_acceptable(int status);
static void clear_fault_environment(const struct p101_env *env, struct p101_error *err);
static void reset_run_environment(const struct p101_env *env, struct p101_error *err);
static void flush_standard_streams(const struct p101_env *env, struct p101_error *err);

int p101_error_path_walk_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct run_result result;
    unsigned int      index;
    size_t            runs;
    size_t            resource_findings;
    bool              trouble;

    P101_TRACE(env);
    runs              = 0;
    resource_findings = 0;
    trouble           = false;

    if(run_one_case(env, err, args, 0, &result) != EXIT_SUCCESS)
    {
        trouble = true;
        goto done;
    }

    runs++;
    p101_error_path_walk_print_run_result(env, err, &result);

    if((int)result.observe_ok == 0 || (!p101_error_path_walk_status_is_success(result.status) && (result.resources.fd_leaks + result.resources.allocation_leaks + result.resources.bad_releases) == 0U))
    {
        trouble = true;
    }

    resource_findings += result.resources.fd_leaks + result.resources.allocation_leaks + result.resources.bad_releases;

    for(index = 1; index <= args->max_failures && p101_error_has_no_error(err); index++)
    {
        if(run_one_case(env, err, args, index, &result) != EXIT_SUCCESS)
        {
            trouble = true;
            goto done;
        }

        runs++;
        p101_error_path_walk_print_run_result(env, err, &result);

        if((int)result.observe_ok == 0)
        {
            trouble = true;
        }

        if((int)result.fault_hit == 0)
        {
            p101_printf(env, err, "p101-error-path-walk: exhausted after %u fault-capable call%s.\n", index - 1U, (index - 1U) == 1U ? "" : "s");
            break;
        }

        resource_findings += result.resources.fd_leaks + result.resources.allocation_leaks + result.resources.bad_releases;
    }

    p101_printf(env, err, "p101-error-path-walk: %zu run%s, %zu resource finding%s.\n", runs, runs == 1 ? "" : "s", resource_findings, resource_findings == 1 ? "" : "s");

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

static int run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    char fault_value[FAULT_LEN];

    P101_TRACE(env);
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
    char   trace_path[PATH_LEN];
    char   report_path[PATH_LEN];
    char   output_option[]  = "-o";
    char   tracker_option[] = "-r";
    char   trace_option[]   = "-t";
    char   report_option[]  = "-p";
    char   separator[]      = "--";
    size_t index;
    int    status;
    pid_t  pid;

    P101_TRACE(env);
    status = 0;
    p101_strncpy(env, observe_path, args->p101_observe, sizeof(observe_path) - 1U);
    observe_path[sizeof(observe_path) - 1U] = '\0';
    p101_strncpy(env, observe_dir, result->observe_dir, sizeof(observe_dir) - 1U);
    observe_dir[sizeof(observe_dir) - 1U] = '\0';
    p101_strncpy(env, tracker_path, args->resource_tracker, sizeof(tracker_path) - 1U);
    tracker_path[sizeof(tracker_path) - 1U] = '\0';
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
    tool_argv[index++] = trace_option;
    tool_argv[index++] = trace_path;
    tool_argv[index++] = report_option;
    tool_argv[index++] = report_path;
    tool_argv[index++] = separator;

    for(size_t i = 0; args->command_argv[i] != NULL && index < MAX_TOOL_ARGS - 1U; i++)
    {
        tool_argv[index++] = args->command_argv[i];
    }
    tool_argv[index] = NULL;

    if(index >= MAX_TOOL_ARGS - 1U)
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
        if(p101_freopen(env, err, result->observe_stdout_path, "w", stdout) == NULL || p101_freopen(env, err, result->observe_stderr_path, "w", stderr) == NULL || p101_error_has_error(err))
        {
            p101_fprintf(env, err, stderr, "p101-error-path-walk: observe setup failed: %s\n", p101_error_get_message(err));
            p101_exit_immediately(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, tool_argv[0], tool_argv);
        p101_fprintf(env, err, stderr, "p101-error-path-walk: exec failed for %s: %s\n", args->p101_observe, p101_error_get_message(err));
        p101_exit_immediately(env, EXEC_FAILURE);
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
    P101_TRACE(env);
    p101_unsetenv(env, err, FAULT_CALL_ENV);
    p101_unsetenv(env, err, FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, FAULT_LOG_ENV);
    p101_unsetenv(env, err, FAULT_NAME_ENV);
    p101_unsetenv(env, err, CALL_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ARGS_ENV);
    p101_unsetenv(env, err, CALL_LOG_RESULT_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_CALL_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_LOG_ENV);
    p101_unsetenv(env, err, CHILD_FAULT_NAME_ENV);
}

static void reset_run_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    clear_fault_environment(env, err);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
}

static void flush_standard_streams(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    p101_fflush(env, err, stdout);

    if(p101_error_has_no_error(err))
    {
        p101_fflush(env, err, stderr);
    }
}
