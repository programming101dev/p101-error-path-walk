#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_stdio.h>
#include <p101_posix/p101_unistd.h>
#include <stdio.h>
#include <string.h>

static void  join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
static char *split_tab(char **cursor);
#ifdef P101_ERROR_PATH_WALK_TESTING
static bool force_error_create_failure;
#endif

void p101_error_path_walk_make_log_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    const char *prefix;
    long        pid_value;

    P101_TRACE_SCOPE(env);
    prefix    = (args->log_prefix == NULL) ? DEFAULT_LOG_PREFIX : args->log_prefix;
    pid_value = (long)p101_getpid(env);

    if(fault_index == 0)
    {
        p101_snprintf(env, err, result->observe_dir, PATH_LEN, "%s-%ld-baseline.observe", prefix, pid_value);
        p101_snprintf(env, err, result->observe_stdout_path, PATH_LEN, "%s-%ld-baseline.observe.stdout.txt", prefix, pid_value);
        p101_snprintf(env, err, result->observe_stderr_path, PATH_LEN, "%s-%ld-baseline.observe.stderr.txt", prefix, pid_value);
    }
    else
    {
        p101_snprintf(env, err, result->observe_dir, PATH_LEN, "%s-%ld-fault-%u.observe", prefix, pid_value, fault_index);
        p101_snprintf(env, err, result->observe_stdout_path, PATH_LEN, "%s-%ld-fault-%u.observe.stdout.txt", prefix, pid_value, fault_index);
        p101_snprintf(env, err, result->observe_stderr_path, PATH_LEN, "%s-%ld-fault-%u.observe.stderr.txt", prefix, pid_value, fault_index);
    }

    result->observe_dir[PATH_LEN - 1]         = '\0';
    result->observe_stdout_path[PATH_LEN - 1] = '\0';
    result->observe_stderr_path[PATH_LEN - 1] = '\0';

    join_path(env, err, result->resource_log_path, result->observe_dir, "resources.log");
    join_path(env, err, result->call_log_path, result->observe_dir, "calls.log");
    join_path(env, err, result->fault_log_path, result->observe_dir, "fault.log");
    join_path(env, err, result->resource_json_path, result->observe_dir, "resource-report.json");
    join_path(env, err, result->report_path, result->observe_dir, "correlated-report.txt");
}

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE_SCOPE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written < 0 || written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "An error-path-walk report path is too long.", ERR_USAGE);
    }
}

bool p101_error_path_walk_file_exists(const struct p101_env *env, const char *path)
{
    struct p101_error *predicate_err;
    FILE              *stream;
    bool               exists;

    P101_TRACE_SCOPE(env);
    exists = false;
    predicate_err =
#ifdef P101_ERROR_PATH_WALK_TESTING
        force_error_create_failure ? NULL :
#endif
                                     p101_error_create(false);

    if(predicate_err == NULL)
    {
        goto done;
    }

    stream = p101_fopen(env, predicate_err, path, "r");

    if(stream != NULL)
    {
        exists = true;
        p101_fclose(env, predicate_err, stream);
    }

done:
    p101_error_destroy(predicate_err);

    return exists;
}

bool p101_error_path_walk_read_fault_hit(const struct p101_env *env, struct p101_error *err, const char *path, char name[NAME_LEN])
{
    struct p101_error *predicate_err;
    FILE              *stream;
    char               line[READ_BUF_LEN];
    bool               hit;

    P101_TRACE_SCOPE(env);
    stream  = NULL;
    hit     = false;
    name[0] = '\0';
    predicate_err =
#ifdef P101_ERROR_PATH_WALK_TESTING
        force_error_create_failure ? NULL :
#endif
                                     p101_error_create(false);
    if(predicate_err == NULL)
    {
        P101_ERROR_RAISE_ERRNO(err, ENOMEM);
        goto done;
    }
    stream = p101_fopen(env, predicate_err, path, "r");
    if(stream == NULL)
    {
        p101_error_destroy(predicate_err);
        goto done;
    }
    p101_error_destroy(predicate_err);

    for(;;)
    {
        char       *cursor;
        const char *field;
        const char *version;
        const char *pid;
        const char *calls_seen;
        const char *errnum;
        const char *mode;
        const char *amount;
        size_t      length;

        if(p101_fgets(env, err, line, sizeof(line), stream) == NULL)
        {
            break;
        }
        length = p101_strlen(env, line);
        if(length == sizeof(line) - 1U && p101_strchr(env, line, '\n') == NULL)
        {
            P101_ERROR_RAISE_USER(err, "The fault log contains an over-long record.", ERR_USAGE);
            goto done;
        }

        cursor = line;
        field  = split_tab(&cursor);

        if(p101_strcmp(env, field, "P101FAULT") != 0)
        {
            continue;
        }

        version    = split_tab(&cursor);
        pid        = split_tab(&cursor);
        calls_seen = split_tab(&cursor);
        field      = split_tab(&cursor);
        errnum     = split_tab(&cursor);
        mode       = NULL;
        amount     = NULL;

        if(version != NULL && p101_strcmp(env, version, "2") == 0)
        {
            mode   = split_tab(&cursor);
            amount = split_tab(&cursor);
        }

        if(version == NULL || pid == NULL || calls_seen == NULL || field == NULL || errnum == NULL || (p101_strcmp(env, version, "2") == 0 && (mode == NULL || amount == NULL)) || cursor != NULL)
        {
            P101_ERROR_RAISE_USER(err, "The fault log contains a malformed P101FAULT record.", ERR_USAGE);
            goto done;
        }

        if(p101_strcmp(env, version, "1") != 0 && p101_strcmp(env, version, "2") != 0)
        {
            P101_ERROR_RAISE_USER(err, "The fault log version is not supported.", ERR_USAGE);
            goto done;
        }

        p101_strncpy(env, name, field, NAME_LEN - 1U);
        name[NAME_LEN - 1U] = '\0';
        hit                 = true;
        break;
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
    else if(*tab == '\n' || *tab == '\r')
    {
        *tab    = '\0';
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

#ifdef P101_ERROR_PATH_WALK_TESTING
void p101_error_path_walk_test_force_error_create_failure(bool force)
{
    force_error_create_failure = force;
}
#endif
