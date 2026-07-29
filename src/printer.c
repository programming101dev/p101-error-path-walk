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

    p101_fputs(env, err, "observe_", stdout);
    p101_error_path_walk_print_status_text(env, err, result->status);

    if(result->resource_log_present && result->resources.parsed)
    {
        p101_printf(env,
                    err,
                    " resources(records=%zu fd_leaks=%zu allocation_leaks=%zu bad_releases=%zu exec_inheritances=%zu generic_resource_leaks=%zu generic_bad_releases=%zu)",
                    result->resources.records,
                    result->resources.fd_leaks,
                    result->resources.allocation_leaks,
                    result->resources.bad_releases,
                    result->resources.exec_inheritances,
                    result->resources.generic_resource_leaks,
                    result->resources.generic_bad_releases);
    }
    else
    {
        p101_fputs(env, err, " resources(unavailable)", stdout);
    }

    p101_printf(env, err, " observe_dir=%s resource_log=%s call_log=%s report=%s\n", result->observe_dir, result->resource_log_path, result->call_log_path, result->report_path);
}

void p101_error_path_walk_print_status_text(const struct p101_env *env, struct p101_error *err, int status)
{
    if(WIFEXITED(status))
    {
        p101_printf(env, err, "exit=%d", WEXITSTATUS(status));
    }
    else if(WIFSIGNALED(status))
    {
        p101_printf(env, err, "signal=%d", WTERMSIG(status));
    }
    else
    {
        p101_printf(env, err, "status=%d", status);
    }
}
