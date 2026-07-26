#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <errno.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_unistd.h>
#include <stdlib.h>

void p101_error_path_walk_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->max_failures       = DEFAULT_MAX_FAILURES;
    args->p101_observe       = DEFAULT_OBSERVE_PATH;
    args->resource_tracker   = DEFAULT_TRACKER_PATH;
    args->p101_trace         = DEFAULT_TRACE_PATH;
    args->p101_report        = DEFAULT_REPORT_PATH;
    args->fault_errno        = EIO;
    args->stop_at_exhaustion = true;
}

void p101_error_path_walk_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvn:l:O:r:t:p:E:F:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                p101_error_path_walk_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'n':
            {
                args->max_failures_str = optarg;
                break;
            }
            case 'l':
            {
                args->log_prefix = optarg;
                break;
            }
            case 'O':
            {
                args->p101_observe = optarg;
                break;
            }
            case 'r':
            {
                args->resource_tracker = optarg;
                break;
            }
            case 't':
            {
                args->p101_trace = optarg;
                break;
            }
            case 'p':
            {
                args->p101_report = optarg;
                break;
            }
            case 'E':
            {
                args->fault_errno_str = optarg;
                break;
            }
            case 'F':
            {
                args->fault_name = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(p101_isprint(env, optopt))
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c' returned by getopt.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_no_error(err))
    {
        args->command_argv = &argv[optind];
    }
}

void p101_error_path_walk_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE(env);

    if(args->command_argv == NULL || args->command_argv[0] == NULL)
    {
        P101_ERROR_RAISE_USER(err, "A command is required.", ERR_USAGE);
        goto done;
    }

    if(args->log_prefix != NULL && args->log_prefix[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The log prefix must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_observe == NULL || args->p101_observe[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-observe path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->resource_tracker == NULL || args->resource_tracker[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-resource-tracker path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_trace == NULL || args->p101_trace[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-trace path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_report == NULL || args->p101_report[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-report path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->fault_name != NULL && args->fault_name[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The fault-name filter must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

void p101_error_path_walk_convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE(env);

    if(args->max_failures_str != NULL)
    {
        args->max_failures = p101_parse_unsigned_int(env, err, args->max_failures_str, DEFAULT_MAX_FAILURES);

        if(p101_error_has_error(err))
        {
            P101_ERROR_RAISE_USER(err, "The failure count must be an unsigned integer.", ERR_USAGE);
            goto done;
        }

        if(args->max_failures > MAX_FAILURES_LIMIT)
        {
            char msg[MSG_LEN];

            p101_snprintf(env, err, msg, sizeof(msg), "The failure count must be at most %u.", (unsigned)MAX_FAILURES_LIMIT);
            P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
            goto done;
        }
    }

    if(args->fault_errno_str != NULL)
    {
        args->fault_errno = p101_parse_positive_int(env, err, args->fault_errno_str, EIO);

        if(p101_error_has_error(err))
        {
            P101_ERROR_RAISE_USER(err, "The injected errno must be a positive integer.", ERR_USAGE);
            goto done;
        }
    }

done:
    return;
}

_Noreturn void p101_error_path_walk_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE(env);

#ifndef P101_SUPPRESS_USAGE_TEXT
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-n <count>] [-l <prefix>] [-O <p101-observe>] [-r <p101-resource-tracker>] [-t <p101-trace>] [-p <p101-report>] [-E <errno>] [-F <name>] -- <command> [args...]\n", program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                      Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                      Enable verbose p101 tracing in the walker\n", stderr);
    p101_fputs(env, err, "  -n <count>              Maximum injected failures to try after the baseline\n", stderr);
    p101_fputs(env, err, "                          (default: 1024, stops early when no fault fires)\n", stderr);
    p101_fputs(env, err, "  -l <prefix>             Prefix for per-run observe directories and fault logs\n", stderr);
    p101_fputs(env, err, "  -O <p101-observe>       p101-observe executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -r <p101-resource-tracker>   p101-resource-tracker executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -t <p101-trace>         p101-trace executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -p <p101-report>        p101-report executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -E <errno>              errno injected by failed wrappers (default: EIO)\n", stderr);
    p101_fputs(env, err, "  -F <name>               Only count/fail matching wrapper names, e.g. open\n", stderr);
    p101_fputs(env, err, "\nThe child must use p101_env_create() from an updated lib_env build.\n", stderr);
#else
    (void)message;
    (void)program_name;
#endif

    p101_exit(env, exit_code);
}
