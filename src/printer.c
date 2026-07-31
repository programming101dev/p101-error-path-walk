#include "printer.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

bool p101_error_path_walk_status_is_success(int status)
{
    bool success;

    success = false;

    if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
    {
        success = true;
    }

    return success;
}

void p101_error_path_walk_print_run_result(const struct p101_env *env, struct p101_error *err, const struct run_result *result)
{
    if(result->fault_index == 0)
    {
        p101_fputs(env, err, "p101-error-path-walk: baseline ", stdout);
    }
    else
    {
        p101_printf(env, err, "p101-error-path-walk: fault %u ", result->fault_index);

        if(result->fault_hit)
        {
            p101_printf(env, err, "hit=%s ", result->fault_name[0] == '\0' ? "?" : result->fault_name);
        }
        else
        {
            p101_fputs(env, err, "no-hit ", stdout);
        }
    }

    p101_fputs(env, err, "pipeline_", stdout);
    p101_error_path_walk_print_status_text(env, err, result->status);

    if(result->resource_log_present && result->resources.parsed)
    {
        p101_printf(env, err, " resources(records=%zu findings=%zu)", result->resources.records, result->resources.findings);
    }
    else
    {
        p101_fputs(env, err, " resources(unavailable)", stdout);
    }
    if(result->analysis.parsed)
    {
        p101_printf(env, err, " analysis(findings=%zu)", result->analysis.findings);
    }
    else
    {
        p101_fputs(env, err, " analysis(unavailable)", stdout);
    }

    p101_printf(env, err, " run_dir=%s capture=%s analysis=%s resource_log=%s call_log=%s report=%s\n", result->run_dir, result->capture_dir, result->analysis_dir, result->resource_log_path, result->call_log_path, result->report_path);
}

void p101_error_path_walk_print_status_text(const struct p101_env *env, struct p101_error *err, int status)
{
    if(WIFEXITED(status))
    {
        p101_printf(env, err, "exit=%d", WEXITSTATUS(status));
    }
    else
    {
        p101_printf(env, err, "signal=%d", WTERMSIG(status));
    }
}
