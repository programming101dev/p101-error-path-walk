#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <errno.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_cli/cli.h>
#include <p101_convert/integer.h>
#include <stdlib.h>

static void handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option, const char *option_argument, int option_character, const char *program_name);
static bool required_text_missing(const char *text);
static bool fault_mode_supported(const struct p101_env *env, const char *mode);
static bool short_io_name_supported(const struct p101_env *env, const char *name);

void p101_error_path_walk_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->max_failures       = DEFAULT_MAX_FAILURES;
    args->p101_run           = DEFAULT_RUN_PATH;
    args->p101_observe       = DEFAULT_OBSERVE_PATH;
    args->p101_analyze       = DEFAULT_ANALYZE_PATH;
    args->event_model        = DEFAULT_MODEL_PATH;
    args->fault_errno        = EIO;
    args->fault_mode         = "error";
    args->fault_amount       = 1U;
    args->fault_repeat       = 1U;
    args->stop_at_exhaustion = true;
}

void p101_error_path_walk_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE_SCOPE(env);
    opterr = 0;

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        p101_error_path_walk_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
    }

    while((opt = p101_getopt(env, argc, argv, ":hvn:l:U:O:Y:B:E:F:M:A:R:")) != -1 && p101_error_has_no_error(err))
    {
        handle_option(env, err, args, opt, optarg, optopt, argv[0]);
    }

    if(p101_error_has_no_error(err))
    {
        args->command_argv = &argv[optind];
    }
}

static void handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option, const char *option_argument, int option_character, const char *program_name)
{
    const char **destination;

    destination = NULL;
    switch(option)
    {
        case 'h':
        {
            p101_error_path_walk_usage(env, err, program_name, EXIT_SUCCESS, NULL);
        }
        case 'v':
        {
            args->verbose = true;
            break;
        }
        case 'n':
            destination = &args->max_failures_str;
            break;
        case 'l':
            destination = &args->log_prefix;
            break;
        case 'U':
            destination = &args->p101_run;
            break;
        case 'O':
            destination = &args->p101_observe;
            break;
        case 'Y':
            destination = &args->p101_analyze;
            break;
        case 'B':
            destination = &args->event_model;
            break;
        case 'E':
            destination = &args->fault_errno_str;
            break;
        case 'F':
            destination = &args->fault_name;
            break;
        case 'M':
            destination = &args->fault_mode;
            break;
        case 'A':
            destination = &args->fault_amount_str;
            break;
        case 'R':
            destination = &args->fault_repeat_str;
            break;
        case ':':
        {
            char msg[MSG_LEN];

            p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", option_character);
            P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
            break;
        }
        case '?':
        {
            char msg[MSG_LEN];

            if(p101_isprint(env, option_character))
            {
                p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", option_character);
            }
            else
            {
                p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)option_character);
            }
            P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
            break;
        }
        default:
        {
            char msg[MSG_LEN];

            p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option value %d returned by getopt.", option);
            P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
            break;
        }
    }
    if(destination != NULL)
    {
        *destination = option_argument;
    }
}

void p101_error_path_walk_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE_SCOPE(env);

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

    if(required_text_missing(args->p101_run))
    {
        P101_ERROR_RAISE_USER(err, "The p101-run path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(required_text_missing(args->p101_observe))
    {
        P101_ERROR_RAISE_USER(err, "The p101-observe path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(required_text_missing(args->p101_analyze))
    {
        P101_ERROR_RAISE_USER(err, "The p101-analyze path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(required_text_missing(args->event_model))
    {
        P101_ERROR_RAISE_USER(err, "The p101-event-model path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->fault_name != NULL && args->fault_name[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The fault-name filter must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!fault_mode_supported(env, args->fault_mode))
    {
        P101_ERROR_RAISE_USER(err, "The fault mode must be error, eintr, timeout, or short.", ERR_USAGE);
        goto done;
    }

    if(p101_strcmp(env, args->fault_mode, "short") == 0 && !short_io_name_supported(env, args->fault_name))
    {
        P101_ERROR_RAISE_USER(err, "Short-I/O mode requires -F read, write, pread, or pwrite.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

static bool required_text_missing(const char *text)
{
    return (text == NULL || text[0] == '\0') != 0;
}

static bool fault_mode_supported(const struct p101_env *env, const char *mode)
{
    return (mode != NULL && (p101_strcmp(env, mode, "error") == 0 || p101_strcmp(env, mode, "eintr") == 0 || p101_strcmp(env, mode, "timeout") == 0 || p101_strcmp(env, mode, "short") == 0)) != 0;
}

static bool short_io_name_supported(const struct p101_env *env, const char *name)
{
    return (name != NULL && (p101_strcmp(env, name, "read") == 0 || p101_strcmp(env, name, "write") == 0 || p101_strcmp(env, name, "pread") == 0 || p101_strcmp(env, name, "pwrite") == 0)) != 0;
}

#ifdef P101_ERROR_PATH_WALK_TESTING
void p101_error_path_walk_test_handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option)
{
    handle_option(env, err, args, option, "value", option, "p101-error-path-walk-test");
}
#endif

void p101_error_path_walk_convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE_SCOPE(env);

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

    if(args->fault_amount_str != NULL)
    {
        args->fault_amount = p101_parse_unsigned_int(env, err, args->fault_amount_str, 1U);
        if(p101_error_has_error(err))
        {
            P101_ERROR_RAISE_USER(err, "The short-I/O amount must be an unsigned integer.", ERR_USAGE);
            goto done;
        }
    }

    if(args->fault_repeat_str != NULL)
    {
        int parsed_repeat;

        parsed_repeat = p101_parse_positive_int(env, err, args->fault_repeat_str, 1);
        if(p101_error_has_error(err))
        {
            P101_ERROR_RAISE_USER(err, "The fault repeat count must be a positive integer.", ERR_USAGE);
            goto done;
        }
        args->fault_repeat = (unsigned)parsed_repeat;
    }

done:
    return;
}

_Noreturn void p101_error_path_walk_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE_SCOPE(env);

#ifndef P101_SUPPRESS_USAGE_TEXT
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env,
                 err,
                 stderr,
                 "Usage: %s [-h] [-v] [-n <count>] [-l <prefix>] [-U <p101-run>] [-O <p101-observe>] [-Y <p101-analyze>] [-B <p101-event-model>] [-E <errno>] [-F <name>] [-M <mode>] [-A <amount>] [-R <count>] -- <command> [args...]\n",
                 program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                      Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                      Enable verbose p101 tracing in the walker\n", stderr);
    p101_fputs(env, err, "  -n <count>              Maximum injected failures to try after the baseline\n", stderr);
    p101_fputs(env, err, "                          (default: 1024, stops early when no fault fires)\n", stderr);
    p101_fputs(env, err, "  -l <prefix>             Prefix for per-case capture and analysis directories\n", stderr);
    p101_fputs(env, err, "  -U <p101-run>           Shared capture/analyze driver (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -O <p101-observe>       p101-observe executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -Y <p101-analyze>       Shared policy-analysis driver (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -B <p101-event-model>   Shared event-model builder (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -E <errno>              errno injected by failed wrappers (default: EIO)\n", stderr);
    p101_fputs(env, err, "  -F <name>               Only count/fail matching wrapper names, e.g. open\n", stderr);
    p101_fputs(env, err, "  -M <mode>               error, eintr, timeout, or short (default: error)\n", stderr);
    p101_fputs(env, err, "  -A <amount>             Maximum bytes for short read/write (default: 1)\n", stderr);
    p101_fputs(env, err, "  -R <count>              Inject at this and the next count-1 matching calls\n", stderr);
    p101_fputs(env, err, "\nThe child must use p101_env_create() from an updated lib_env build.\n", stderr);
#else
    (void)message;
    (void)program_name;
#endif

    p101_exit(env, exit_code);
}
