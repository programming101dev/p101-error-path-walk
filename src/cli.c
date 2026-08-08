#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <errno.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_cli/p101_getopt.h>
#include <p101_cli/p101_stdlib.h>
#include <p101_cli/p101_unistd.h>
#include <p101_convert/integer.h>
#include <stdlib.h>

static void handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option, const char *option_argument, int option_character);
static bool required_text_missing(const char *text);

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
    int  p101_expression_result_12;
    int  p101_call_result_13;
    bool p101_call_result_1;
    int  opt;

    P101_TRACE_SCOPE(env);
    opterr = 0;

    p101_expression_result_12 = 0;
    if(argc == 2)
    {
        p101_call_result_13 = p101_strcmp(env, argv[1], "--help");
        if(p101_call_result_13 == 0)
        {
            p101_expression_result_12 = 1;
        }
    }
    if(p101_expression_result_12)
    {
        args->show_help = true;
        goto done;
    }

    for(;;)
    {
        opt = p101_getopt(env, argc, argv, ":hvn:l:U:O:Y:B:E:F:M:A:R:");
        if(opt == -1)
        {
            break;
        }
        p101_call_result_1 = p101_error_has_no_error(err);
        if(!p101_call_result_1)
        {
            break;
        }
        handle_option(env, err, args, opt, optarg, optopt);
    }

    p101_call_result_1 = p101_error_has_no_error(err);
    if(p101_call_result_1)
    {
        args->command_argv = &argv[optind];
    }

done:
    return;
}

static void handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option, const char *option_argument, int option_character)
{
    const char **destination;

    destination = NULL;
    switch(option)
    {
        case 'h':
        {
            args->show_help = true;
            break;
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
            int  p101_call_result_2;
            char msg[MSG_LEN];

            p101_call_result_2 = p101_isprint(env, option_character);
            if(p101_call_result_2)
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
    p101_env_fault_mode mode;
    bool                p101_call_result_7;
    bool                p101_call_result_3;
    bool                p101_call_result_4;
    bool                p101_call_result_5;
    bool                p101_call_result_6;
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

    p101_call_result_3 = required_text_missing(args->p101_run);
    if(p101_call_result_3)
    {
        P101_ERROR_RAISE_USER(err, "The p101-run path must not be empty.", ERR_USAGE);
        goto done;
    }

    p101_call_result_4 = required_text_missing(args->p101_observe);
    if(p101_call_result_4)
    {
        P101_ERROR_RAISE_USER(err, "The p101-observe path must not be empty.", ERR_USAGE);
        goto done;
    }

    p101_call_result_5 = required_text_missing(args->p101_analyze);
    if(p101_call_result_5)
    {
        P101_ERROR_RAISE_USER(err, "The p101-analyze path must not be empty.", ERR_USAGE);
        goto done;
    }

    p101_call_result_6 = required_text_missing(args->event_model);
    if(p101_call_result_6)
    {
        P101_ERROR_RAISE_USER(err, "The p101-event-model path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->fault_name != NULL && args->fault_name[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The fault-name filter must not be empty.", ERR_USAGE);
        goto done;
    }

    p101_call_result_7 = p101_env_fault_mode_from_name(args->fault_mode, &mode);
    if(!p101_call_result_7)
    {
        P101_ERROR_RAISE_USER(err, "The fault mode must be error, eintr, timeout, short, or uncertain.", ERR_USAGE);
        goto done;
    }

    if((mode == P101_ENV_FAULT_MODE_SHORT || mode == P101_ENV_FAULT_MODE_UNCERTAIN) && args->fault_name == NULL)
    {
        P101_ERROR_RAISE_USER(err, "Short and uncertain modes require an exact wrapper identity with -F.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

static bool required_text_missing(const char *text)
{
    return (text == NULL || text[0] == '\0') != 0;
}

#ifdef P101_ERROR_PATH_WALK_TESTING
void p101_error_path_walk_test_handle_option(const struct p101_env *env, struct p101_error *err, struct arguments *args, int option)
{
    handle_option(env, err, args, option, "value", option);
}
#endif

void p101_error_path_walk_convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    bool p101_call_result_8;
    bool p101_call_result_9;
    bool p101_call_result_10;
    bool p101_call_result_11;
    P101_TRACE_SCOPE(env);

    if(args->max_failures_str != NULL)
    {
        args->max_failures = p101_parse_unsigned_int(env, err, args->max_failures_str, DEFAULT_MAX_FAILURES);

        p101_call_result_8 = p101_error_has_error(err);
        if(p101_call_result_8)
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

        p101_call_result_9 = p101_error_has_error(err);
        if(p101_call_result_9)
        {
            P101_ERROR_RAISE_USER(err, "The injected errno must be a positive integer.", ERR_USAGE);
            goto done;
        }
    }

    if(args->fault_amount_str != NULL)
    {
        args->fault_amount  = p101_parse_unsigned_int(env, err, args->fault_amount_str, 1U);
        p101_call_result_10 = p101_error_has_error(err);
        if(p101_call_result_10)
        {
            P101_ERROR_RAISE_USER(err, "The short-I/O amount must be an unsigned integer.", ERR_USAGE);
            goto done;
        }
    }

    if(args->fault_repeat_str != NULL)
    {
        int parsed_repeat;

        parsed_repeat       = p101_parse_positive_int(env, err, args->fault_repeat_str, 1);
        p101_call_result_11 = p101_error_has_error(err);
        if(p101_call_result_11)
        {
            P101_ERROR_RAISE_USER(err, "The fault repeat count must be a positive integer.", ERR_USAGE);
            goto done;
        }
        args->fault_repeat = (unsigned)parsed_repeat;
    }

done:
    return;
}

void p101_error_path_walk_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE_SCOPE(env);
    (void)exit_code;

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
    p101_fputs(env, err, "  -F <name>               Exact public wrapper identity, e.g. p101_open\n", stderr);
    p101_fputs(env, err, "  -M <mode>               error, eintr, timeout, short, or uncertain (default: error)\n", stderr);
    p101_fputs(env, err, "  -A <amount>             Maximum bytes for short read/write (default: 1)\n", stderr);
    p101_fputs(env, err, "  -R <count>              Inject at this and the next count-1 matching calls\n", stderr);
    p101_fputs(env, err, "\nThe child must use p101_env_create() from an updated lib_env build.\n", stderr);
#else
    (void)exit_code;
    (void)message;
    (void)program_name;
#endif
}
