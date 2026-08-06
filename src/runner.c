#include "runner.h"
#include "constants.h"
#include "errors.h"
#include "paths.h"
#include "printer.h"
#include "resource.h"
#include "result.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_process/p101_sched.h>
#include <p101_process/p101_setjmp.h>
#include <p101_process/p101_signal.h>
#include <p101_process/p101_spawn.h>
#include <p101_process/p101_stdio.h>
#include <p101_process/p101_stdlib.h>
#include <p101_process/p101_unistd.h>
#include <p101_process/sys/p101_resource.h>
#include <p101_process/sys/p101_times.h>
#include <p101_process/sys/p101_wait.h>
#include <p101_util/tool_run.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

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
static int    run_p101_pipeline(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result);
static void   update_fault_group(const struct p101_env *env, struct p101_error *err, struct fault_group groups[FAULT_GROUP_LIMIT], size_t *group_count, const struct run_result *result);
static void   print_fault_groups(const struct p101_env *env, struct p101_error *err, const struct fault_group groups[FAULT_GROUP_LIMIT], size_t group_count);
static size_t analysis_finding_count(const struct run_result *result);
static bool   analysis_summary_unavailable(const struct run_result *result);
static bool   pipeline_status_is_acceptable(int status);
static void   clear_fault_environment(const struct p101_env *env, struct p101_error *err);
static void   reset_run_environment(const struct p101_env *env, struct p101_error *err);

int p101_error_path_walk_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    int                p101_expression_result_12;
    int                p101_expression_result_13;
    bool               p101_call_result_14;
    int                p101_expression_result_15;
    bool               p101_call_result_16;
    size_t             p101_call_result_17;
    int                p101_expression_result_18;
    bool               p101_call_result_19;
    int                p101_expression_result_20;
    bool               p101_call_result_21;
    int                p101_call_result_1;
    size_t             p101_call_result_2;
    int                p101_call_result_3;
    size_t             p101_call_result_4;
    struct run_result  result;
    unsigned int       index;
    size_t             runs;
    size_t             findings;
    struct fault_group groups[FAULT_GROUP_LIMIT];
    size_t             group_count;
    bool               trouble;
    bool               no_error;
    int                status;

    P101_TRACE_SCOPE(env);
    runs        = 0;
    findings    = 0;
    group_count = 0;
    p101_memset(env, groups, 0, sizeof(groups));
    trouble = false;

    p101_call_result_1 = run_one_case(env, err, args, 0, &result);
    if(p101_call_result_1 != EXIT_SUCCESS)
    {
        trouble = true;
        goto done;
    }

    runs++;
    p101_error_path_walk_print_run_result(env, err, &result);

    if((int)result.pipeline_ok == 0)
    {
        p101_expression_result_13 = 1;
    }
    else
    {
        p101_call_result_14 = analysis_summary_unavailable(&result);
        if(p101_call_result_14)
        {
            p101_expression_result_13 = 1;
        }
        else
        {
            p101_expression_result_13 = 0;
        }
    }
    if(p101_expression_result_13)
    {
        p101_expression_result_12 = 1;
    }
    else
    {
        p101_call_result_16       = p101_error_path_walk_status_is_success(result.status);
        p101_expression_result_15 = 0;
        if(!p101_call_result_16)
        {
            p101_call_result_17 = analysis_finding_count(&result);
            if(p101_call_result_17 == 0U)
            {
                p101_expression_result_15 = 1;
            }
        }
        if(p101_expression_result_15)
        {
            p101_expression_result_12 = 1;
        }
        else
        {
            p101_expression_result_12 = 0;
        }
    }
    if(p101_expression_result_12)
    {
        trouble = true;
    }

    p101_call_result_2 = analysis_finding_count(&result);
    findings += p101_call_result_2;

    for(index = 1; index <= args->max_failures; index++)
    {
        no_error = p101_error_has_no_error(err);
        if(!no_error)
        {
            break;
        }
        p101_call_result_3 = run_one_case(env, err, args, index, &result);
        if(p101_call_result_3 != EXIT_SUCCESS)
        {
            trouble = true;
            goto done;
        }

        runs++;
        p101_error_path_walk_print_run_result(env, err, &result);

        if((int)result.pipeline_ok == 0)
        {
            p101_expression_result_18 = 1;
        }
        else
        {
            p101_call_result_19 = analysis_summary_unavailable(&result);
            if(p101_call_result_19)
            {
                p101_expression_result_18 = 1;
            }
            else
            {
                p101_expression_result_18 = 0;
            }
        }
        if(p101_expression_result_18)
        {
            trouble = true;
        }

        if((int)result.fault_hit == 0)
        {
            p101_printf(env, err, "p101-error-path-walk: exhausted after %u fault-capable call%s.\n", index - 1U, (index - 1U) == 1U ? "" : "s");
            break;
        }

        p101_call_result_4 = analysis_finding_count(&result);
        findings += p101_call_result_4;
        update_fault_group(env, err, groups, &group_count, &result);
    }

    p101_printf(env, err, "p101-error-path-walk: %zu run%s, %zu policy finding%s.\n", runs, runs == 1 ? "" : "s", findings, findings == 1 ? "" : "s");
    print_fault_groups(env, err, groups, group_count);

done:
    reset_run_environment(env, err);

    p101_call_result_21 = p101_error_has_error(err);
    if(p101_call_result_21)
    {
        p101_expression_result_20 = 1;
    }
    else
    {
        if(trouble)
        {
            p101_expression_result_20 = 1;
        }
        else
        {
            p101_expression_result_20 = 0;
        }
    }
    if(p101_expression_result_20)
    {
        status = EXIT_TROUBLE;
    }
    else if(findings > 0)
    {
        status = EXIT_FINDINGS;
    }
    else
    {
        status = EXIT_SUCCESS;
    }

    return status;
}

static void update_fault_group(const struct p101_env *env, struct p101_error *err, struct fault_group groups[FAULT_GROUP_LIMIT], size_t *group_count, const struct run_result *result)
{
    int         p101_call_result_5;
    const char *name;
    size_t      findings;
    size_t      index;

    if(result->fault_index == 0 || !result->fault_hit)
    {
        goto done;
    }

    name     = (result->fault_name[0] == '\0') ? "?" : result->fault_name;
    findings = analysis_finding_count(result);
    index    = *group_count;

    for(size_t i = 0; i < *group_count; i++)
    {
        p101_call_result_5 = p101_strcmp(env, groups[i].name, name);
        if(p101_call_result_5 == 0)
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
        p101_printf(env, err, "  %s: %zu run%s, %zu policy finding%s\n", groups[i].name, groups[i].runs, groups[i].runs == 1U ? "" : "s", groups[i].findings, groups[i].findings == 1U ? "" : "s");
    }

done:
    return;
}

static size_t analysis_finding_count(const struct run_result *result)
{
    size_t findings;

    if(!result->analysis.parsed)
    {
        findings = 0U;
    }
    else
    {
        findings = result->analysis.findings;
    }

    return findings;
}

static bool analysis_summary_unavailable(const struct run_result *result)
{
    return (!result->resource_log_present || !result->resources.parsed || !result->resources.has_records || !result->analysis.parsed) != 0;
}

static int run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    int  p101_expression_result_22;
    bool p101_call_result_23;
    bool p101_call_result_6;
    bool p101_call_result_7;
    bool p101_call_result_8;
    bool p101_call_result_9;
    bool p101_call_result_10;
    bool p101_call_result_11;
    char fault_value[FAULT_LEN];
    int  status;

    P101_TRACE_SCOPE(env);
    p101_memset(env, result, 0, sizeof(*result));
    result->fault_index = fault_index;
    p101_error_path_walk_make_log_paths(env, err, args, fault_index, result);

    p101_call_result_6 = p101_error_has_error(err);
    if(p101_call_result_6)
    {
        goto done;
    }

    clear_fault_environment(env, err);

    p101_call_result_7 = p101_error_has_error(err);
    if(p101_call_result_7)
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

    p101_call_result_8 = p101_error_has_error(err);
    if(p101_call_result_8)
    {
        goto done;
    }

    result->status      = run_p101_pipeline(env, err, args, result);
    result->pipeline_ok = pipeline_status_is_acceptable(result->status);
    result->fault_hit   = p101_error_path_walk_read_fault_hit(env, err, result->fault_log_path, result->fault_name);

    p101_call_result_9 = p101_error_has_error(err);
    if(p101_call_result_9)
    {
        goto done;
    }

    result->resource_log_present = p101_error_path_walk_file_exists(env, result->resource_log_path);

    p101_expression_result_22 = 0;
    if(result->resource_log_present)
    {
        p101_call_result_23 = p101_error_path_walk_file_exists(env, result->resource_json_path);
        if(p101_call_result_23)
        {
            p101_expression_result_22 = 1;
        }
    }
    if(p101_expression_result_22)
    {
        p101_error_path_walk_read_policy_json(env, err, result->resource_json_path, RESOURCE_POLICY_SCHEMA, &result->resources);
    }

    p101_call_result_10 = p101_error_path_walk_file_exists(env, result->analysis_json_path);
    if(p101_call_result_10)
    {
        p101_error_path_walk_read_policy_json(env, err, result->analysis_json_path, ANALYSIS_POLICY_SCHEMA, &result->analysis);
    }

done:
    clear_fault_environment(env, err);

    p101_call_result_11 = p101_error_has_error(err);
    if(p101_call_result_11)
    {
        status = EXIT_TROUBLE;
    }
    else
    {
        status = EXIT_SUCCESS;
    }

    return status;
}

static int run_p101_pipeline(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result)
{
    char  *tool_argv[MAX_TOOL_ARGS];
    char   run_path[PATH_LEN];
    char   run_dir[PATH_LEN];
    char   observe_path[PATH_LEN];
    char   analyze_path[PATH_LEN];
    char   model_path[PATH_LEN];
    char   output_option[]  = "-o";
    char   observe_option[] = "--observe-tool";
    char   analyze_option[] = "--analyze-tool";
    char   model_option[]   = "--model-tool";
    char   separator[]      = "--";
    size_t index;
    size_t command_index;
    int    status;

    P101_TRACE_SCOPE(env);
    status = 0;
    p101_strncpy(env, run_path, args->p101_run, sizeof(run_path) - 1U);
    run_path[sizeof(run_path) - 1U] = '\0';
    p101_strncpy(env, run_dir, result->run_dir, sizeof(run_dir) - 1U);
    run_dir[sizeof(run_dir) - 1U] = '\0';
    p101_strncpy(env, observe_path, args->p101_observe, sizeof(observe_path) - 1U);
    observe_path[sizeof(observe_path) - 1U] = '\0';
    p101_strncpy(env, analyze_path, args->p101_analyze, sizeof(analyze_path) - 1U);
    analyze_path[sizeof(analyze_path) - 1U] = '\0';
    p101_strncpy(env, model_path, args->event_model, sizeof(model_path) - 1U);
    model_path[sizeof(model_path) - 1U] = '\0';

    index              = 0;
    tool_argv[index++] = run_path;
    tool_argv[index++] = output_option;
    tool_argv[index++] = run_dir;
    tool_argv[index++] = observe_option;
    tool_argv[index++] = observe_path;
    tool_argv[index++] = analyze_option;
    tool_argv[index++] = analyze_path;
    tool_argv[index++] = model_option;
    tool_argv[index++] = model_path;
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

    {
        struct p101_tool_run_options options;

        options.stdout_path         = result->pipeline_stdout_path;
        options.stderr_path         = result->pipeline_stderr_path;
        options.diagnostic_name     = "p101-error-path-walk";
        options.output_mode         = REPORT_FILE_MODE;
        options.child_setup         = NULL;
        options.child_setup_context = NULL;
        status                      = p101_tool_run_capture(env, err, tool_argv, &options);
    }

done:
    return status;
}

static bool pipeline_status_is_acceptable(int status)
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

#ifdef P101_ERROR_PATH_WALK_TESTING
int p101_error_path_walk_test_run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    return run_one_case(env, err, args, fault_index, result);
}

int p101_error_path_walk_test_run_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct run_result *result)
{
    return run_p101_pipeline(env, err, args, result);
}

bool p101_error_path_walk_test_observe_status(int status)
{
    return pipeline_status_is_acceptable(status);
}

size_t p101_error_path_walk_test_analysis_finding_count(const struct run_result *result)
{
    return analysis_finding_count(result);
}

bool p101_error_path_walk_test_analysis_summary_unavailable(const struct run_result *result)
{
    return analysis_summary_unavailable(result);
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
    result.resources.findings = 1U;
    result.analysis.parsed    = true;
    result.analysis.findings  = 1U;
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
