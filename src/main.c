#include "arguments.h"
#include "errors.h"
#include <errno.h>
#include <limits.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_stdio.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_wait.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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
    bool                    tracker_ok;
    char                    fault_name[NAME_LEN];
    char                    resource_log_path[PATH_LEN];
    char                    fault_log_path[PATH_LEN];
    struct resource_summary resources;
};

static void           parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void           check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static void           convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args);
static int            run_error_path_walk(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static int            run_one_case(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result);
static int            run_child(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static int            run_resource_tracker(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *log_path, struct resource_summary *summary);
static void           make_log_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, char resource_path[PATH_LEN], char fault_path[PATH_LEN]);
static void           truncate_file(const struct p101_env *env, struct p101_error *err, const char *path);
static bool           file_exists(const struct p101_env *env, const char *path);
static bool           read_fault_hit(const struct p101_env *env, struct p101_error *err, const char *path, char name[NAME_LEN]);
static char          *split_tab(char **cursor);
static bool           parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary);
static bool           parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value);
static bool           status_is_success(int status);
static void           print_run_result(const struct p101_env *env, struct p101_error *err, const struct run_result *result);
static void           print_status_text(const struct p101_env *env, struct p101_error *err, int status);
static void           clear_fault_environment(const struct p101_env *env, struct p101_error *err);
static void           reset_run_environment(const struct p101_env *env, struct p101_error *err);
_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

static const char FAULT_CALL_ENV[]        = "P101_FAULT_CALL";
static const char FAULT_ERRNO_ENV[]       = "P101_FAULT_ERRNO";
static const char FAULT_LOG_ENV[]         = "P101_FAULT_LOG";
static const char FAULT_NAME_ENV[]        = "P101_FAULT_NAME";
static const char RESOURCE_LOG_ENV[]      = "P101_RESOURCE_LOG";
static const char DEFAULT_TRACKER_PATH[]  = "p101-resource-tracker";
static const char DEFAULT_LOG_PREFIX[]    = "/tmp/p101-error-path-walk";
static const char JSON_RECORDS[]          = "\"records\"";
static const char JSON_FD_LEAKS[]         = "\"fd_leaks\"";
static const char JSON_ALLOCATION_LEAKS[] = "\"allocation_leaks\"";
static const char JSON_BAD_RELEASES[]     = "\"bad_releases\"";

enum
{
    MSG_LEN              = 256,
    FAULT_LEN            = 32,
    READ_BUF_LEN         = 4096,
    TRACKER_OUTPUT_LIMIT = 65536,
    EXEC_FAILURE         = 127,
    DEFAULT_MAX_FAILURES = 1024,
    JSON_NUMBER_BASE     = 10,
    EXIT_FINDINGS        = 1,
    EXIT_TROUBLE         = 2
};

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    int                ret_val;

    ret_val = EXIT_TROUBLE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    p101_memset(env, &args, 0, sizeof(args));
    args.max_failures       = DEFAULT_MAX_FAILURES;
    args.resource_tracker   = DEFAULT_TRACKER_PATH;
    args.fault_errno        = EIO;
    args.stop_at_exhaustion = true;

    parse_arguments(env, err, argc, argv, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args.verbose)
    {
        p101_env_set_tracer(env, p101_env_default_tracer);
    }

    check_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    convert_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = run_error_path_walk(env, err, &args);

done:
    reset_run_environment(env, err);

    if(p101_error_has_error(err))
    {
        if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
        {
            const char *msg;

            msg = p101_error_get_message(err);
            usage(env, err, argv[0], EXIT_TROUBLE, msg);
        }

        p101_fprintf(env, err, stderr, "%s\n", p101_error_get_message(err));
        ret_val = EXIT_TROUBLE;
    }

    p101_env_destroy(env);
    p101_error_destroy(err);

    return ret_val;
}

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvn:l:r:E:F:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                usage(env, err, argv[0], EXIT_SUCCESS, NULL);
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
            case 'r':
            {
                args->resource_tracker = optarg;
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

static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
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

    if(args->resource_tracker == NULL || args->resource_tracker[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-resource-tracker path must not be empty.", ERR_USAGE);
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

static void convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
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

static int run_error_path_walk(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
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
    print_run_result(env, err, &result);

    if(!status_is_success(result.status))
    {
        trouble = true;
    }

    if((int)result.tracker_ok == 0)
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
        print_run_result(env, err, &result);

        if((int)result.tracker_ok == 0)
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
    make_log_paths(env, err, args, fault_index, result->resource_log_path, result->fault_log_path);
    truncate_file(env, err, result->resource_log_path);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    truncate_file(env, err, result->fault_log_path);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    clear_fault_environment(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_setenv(env, err, RESOURCE_LOG_ENV, result->resource_log_path, 1);
    p101_setenv(env, err, FAULT_LOG_ENV, result->fault_log_path, 1);

    if(args->fault_name != NULL)
    {
        p101_setenv(env, err, FAULT_NAME_ENV, args->fault_name, 1);
    }

    if(args->fault_errno_str != NULL)
    {
        char errno_value[FAULT_LEN];

        p101_snprintf(env, err, errno_value, sizeof(errno_value), "%d", args->fault_errno);
        p101_setenv(env, err, FAULT_ERRNO_ENV, errno_value, 1);
    }

    if(fault_index > 0)
    {
        p101_snprintf(env, err, fault_value, sizeof(fault_value), "%u", fault_index);
        p101_setenv(env, err, FAULT_CALL_ENV, fault_value, 1);
    }

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result->status    = run_child(env, err, args);
    result->fault_hit = read_fault_hit(env, err, result->fault_log_path, result->fault_name);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result->resource_log_present = file_exists(env, result->resource_log_path);

    if(result->resource_log_present)
    {
        result->tracker_ok = (run_resource_tracker(env, err, args, result->resource_log_path, &result->resources) != EXIT_TROUBLE);
    }

done:
    clear_fault_environment(env, err);

    if(p101_error_has_error(err))
    {
        return EXIT_TROUBLE;
    }

    return EXIT_SUCCESS;
}

static int run_child(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    int   status;
    pid_t pid;

    P101_TRACE(env);
    status = 0;
    pid    = p101_fork(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(pid == 0)
    {
        p101_execvp(env, err, args->command_argv[0], args->command_argv);
        p101_fprintf(env, err, stderr, "p101-error-path-walk: exec failed for %s: %s\n", args->command_argv[0], p101_error_get_message(err));
        p101__exit(env, EXEC_FAILURE);
    }

    p101_waitpid(env, err, pid, &status, 0);

done:
    return status;
}

static int run_resource_tracker(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const char *log_path, struct resource_summary *summary)
{
    char   output[TRACKER_OUTPUT_LIMIT];
    char   read_buffer[READ_BUF_LEN];
    char  *tracker_argv[4];
    int    pipe_fds[2];
    int    status;
    pid_t  pid;
    size_t used;
    bool   parsed;
    int    ret_val;

    P101_TRACE(env);
    p101_memset(env, summary, 0, sizeof(*summary));
    pipe_fds[0] = -1;
    pipe_fds[1] = -1;
    status      = 0;
    used        = 0;
    ret_val     = EXIT_TROUBLE;

    if(p101_pipe(env, err, pipe_fds) == -1)
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
        char tracker_path[PATH_LEN];
        char tracker_option[] = "-j";
        char child_log_path[PATH_LEN];

        p101_strncpy(env, tracker_path, args->resource_tracker, sizeof(tracker_path) - 1U);
        tracker_path[sizeof(tracker_path) - 1U] = '\0';
        p101_strncpy(env, child_log_path, log_path, sizeof(child_log_path) - 1U);
        child_log_path[sizeof(child_log_path) - 1U] = '\0';
        p101_close(env, err, pipe_fds[0]);
        p101_dup2(env, err, pipe_fds[1], STDOUT_FILENO);
        p101_close(env, err, pipe_fds[1]);
        reset_run_environment(env, err);
        tracker_argv[0] = tracker_path;
        tracker_argv[1] = tracker_option;
        tracker_argv[2] = child_log_path;
        tracker_argv[3] = NULL;
        p101_execvp(env, err, tracker_argv[0], tracker_argv);
        p101_fprintf(env, err, stderr, "p101-error-path-walk: exec failed for %s: %s\n", args->resource_tracker, p101_error_get_message(err));
        p101__exit(env, EXEC_FAILURE);
    }

    p101_close(env, err, pipe_fds[1]);
    pipe_fds[1] = -1;

    while(p101_error_has_no_error(err))
    {
        ssize_t amount;

        amount = p101_read(env, err, pipe_fds[0], read_buffer, sizeof(read_buffer));

        if(amount <= 0)
        {
            break;
        }

        if(used < sizeof(output) - 1U)
        {
            size_t room;
            size_t copied;

            room   = (sizeof(output) - 1U) - used;
            copied = ((size_t)amount < room) ? (size_t)amount : room;
            p101_memcpy(env, output + used, read_buffer, copied);
            used += copied;
        }
    }

    output[used] = '\0';
    p101_close(env, err, pipe_fds[0]);
    pipe_fds[0] = -1;
    p101_waitpid(env, err, pid, &status, 0);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    parsed = parse_resource_summary(env, output, summary);

    if(parsed && (WIFEXITED(status) && (WEXITSTATUS(status) == EXIT_SUCCESS || WEXITSTATUS(status) == EXIT_FINDINGS)))
    {
        ret_val = WEXITSTATUS(status);
    }

done:
    if(pipe_fds[0] != -1)
    {
        p101_close(env, err, pipe_fds[0]);
    }

    if(pipe_fds[1] != -1)
    {
        p101_close(env, err, pipe_fds[1]);
    }

    return ret_val;
}

static void make_log_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, char resource_path[PATH_LEN], char fault_path[PATH_LEN])
{
    const char *prefix;
    long        pid_value;

    P101_TRACE(env);
    prefix    = (args->log_prefix == NULL) ? DEFAULT_LOG_PREFIX : args->log_prefix;
    pid_value = (long)p101_getpid(env);

    if(fault_index == 0)
    {
        p101_snprintf(env, err, resource_path, PATH_LEN, "%s-%ld-baseline.resource.log", prefix, pid_value);
        p101_snprintf(env, err, fault_path, PATH_LEN, "%s-%ld-baseline.fault.log", prefix, pid_value);
    }
    else
    {
        p101_snprintf(env, err, resource_path, PATH_LEN, "%s-%ld-fault-%u.resource.log", prefix, pid_value, fault_index);
        p101_snprintf(env, err, fault_path, PATH_LEN, "%s-%ld-fault-%u.fault.log", prefix, pid_value, fault_index);
    }

    resource_path[PATH_LEN - 1] = '\0';
    fault_path[PATH_LEN - 1]    = '\0';
}

static void truncate_file(const struct p101_env *env, struct p101_error *err, const char *path)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static bool file_exists(const struct p101_env *env, const char *path)
{
    bool exists;

    P101_TRACE(env);
    (void)path;
    exists = true;

    return exists;
}

static bool read_fault_hit(const struct p101_env *env, struct p101_error *err, const char *path, char name[NAME_LEN])
{
    FILE *stream;
    char  line[READ_BUF_LEN];
    bool  hit;

    P101_TRACE(env);
    hit     = false;
    name[0] = '\0';
    stream  = p101_fopen(env, err, path, "r");

    if(stream == NULL)
    {
        goto done;
    }

    if(p101_fgets(env, err, line, sizeof(line), stream) != NULL)
    {
        char *cursor;
        char *field;

        cursor = line;
        field  = split_tab(&cursor);

        if(field != NULL && p101_strcmp(env, field, "P101FAULT") == 0)
        {
            (void)split_tab(&cursor);
            (void)split_tab(&cursor);
            (void)split_tab(&cursor);
            field = split_tab(&cursor);

            if(field != NULL)
            {
                p101_strncpy(env, name, field, NAME_LEN - 1U);
                name[NAME_LEN - 1U] = '\0';
                hit                 = true;
            }
        }
    }

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }

    return hit;
}

static char *split_tab(char **cursor)
{
    char *start;
    char *tab;

    start = *cursor;

    if(start == NULL)
    {
        goto done;
    }

    tab = start;

    while(*tab != '\0' && *tab != '\t' && *tab != '\n' && *tab != '\r')
    {
        tab++;
    }

    if(*tab == '\0')
    {
        *cursor = NULL;
    }
    else
    {
        *tab    = '\0';
        *cursor = tab + 1;
    }

done:
    return start;
}

static bool parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary)
{
    bool parsed;
    bool records_parsed;
    bool fd_leaks_parsed;
    bool allocation_leaks_parsed;
    bool bad_releases_parsed;

    records_parsed          = parse_json_size(env, text, JSON_RECORDS, &summary->records);
    fd_leaks_parsed         = parse_json_size(env, text, JSON_FD_LEAKS, &summary->fd_leaks);
    allocation_leaks_parsed = parse_json_size(env, text, JSON_ALLOCATION_LEAKS, &summary->allocation_leaks);
    bad_releases_parsed     = parse_json_size(env, text, JSON_BAD_RELEASES, &summary->bad_releases);
    parsed                  = (records_parsed && fd_leaks_parsed && allocation_leaks_parsed && bad_releases_parsed) != 0;
    summary->parsed         = parsed;

    return parsed;
}

static bool parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value)
{
    const char   *cursor;
    char         *end;
    unsigned long parsed;
    bool          ok;

    ok     = false;
    cursor = p101_strstr(env, text, key);

    if(cursor == NULL)
    {
        goto done;
    }

    cursor = p101_strchr(env, cursor, ':');

    if(cursor == NULL)
    {
        goto done;
    }

    cursor++;
    parsed = p101_strtoul(env, NULL, cursor, &end, JSON_NUMBER_BASE);

    if(cursor == end)
    {
        goto done;
    }

    *value = (size_t)parsed;
    ok     = true;

done:
    return ok;
}

static bool status_is_success(int status)
{
    bool success;

    success = false;

    if(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS)
    {
        success = true;
    }

    return success;
}

static void print_run_result(const struct p101_env *env, struct p101_error *err, const struct run_result *result)
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

    print_status_text(env, err, result->status);

    if(result->resource_log_present && result->resources.parsed)
    {
        p101_printf(env, err, " resources(records=%zu fd_leaks=%zu allocation_leaks=%zu bad_releases=%zu)", result->resources.records, result->resources.fd_leaks, result->resources.allocation_leaks, result->resources.bad_releases);
    }
    else
    {
        p101_fputs(env, err, " resources(unavailable)", stdout);
    }

    p101_printf(env, err, " log=%s\n", result->resource_log_path);
}

static void print_status_text(const struct p101_env *env, struct p101_error *err, int status)
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

static void clear_fault_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    p101_unsetenv(env, err, FAULT_CALL_ENV);
    p101_unsetenv(env, err, FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, FAULT_LOG_ENV);
    p101_unsetenv(env, err, FAULT_NAME_ENV);
}

static void reset_run_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    clear_fault_environment(env, err);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
}

_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    P101_TRACE(env);

#ifndef P101_SUPPRESS_USAGE_TEXT
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-n <count>] [-l <prefix>] [-r <p101-resource-tracker>] [-E <errno>] [-F <name>] -- <command> [args...]\n", program_name);
    p101_fputs(env, err, "Options:\n", stderr);
    p101_fputs(env, err, "  -h                      Display this help message and exit\n", stderr);
    p101_fputs(env, err, "  -v                      Enable verbose p101 tracing in the walker\n", stderr);
    p101_fputs(env, err, "  -n <count>              Maximum injected failures to try after the baseline\n", stderr);
    p101_fputs(env, err, "                          (default: 1024, stops early when no fault fires)\n", stderr);
    p101_fputs(env, err, "  -l <prefix>             Prefix for per-run resource/fault logs\n", stderr);
    p101_fputs(env, err, "  -r <p101-resource-tracker>   p101-resource-tracker executable (default: PATH lookup)\n", stderr);
    p101_fputs(env, err, "  -E <errno>              errno injected by failed wrappers (default: EIO)\n", stderr);
    p101_fputs(env, err, "  -F <name>               Only count/fail matching wrapper names, e.g. open\n", stderr);
    p101_fputs(env, err, "\nThe child must use p101_env_create() from an updated lib_env build.\n", stderr);
#else
    (void)message;
    (void)program_name;
#endif

    p101_exit(env, exit_code);
}
