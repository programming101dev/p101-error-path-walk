#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_stdio.h>
#include <p101_posix/p101_unistd.h>
#include <stdio.h>
#include <string.h>

static void  join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
static char *split_tab(char **cursor);

void p101_error_path_walk_make_log_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    const char *prefix;
    long        pid_value;

    P101_TRACE(env);
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

    P101_TRACE(env);
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

    P101_TRACE(env);
    exists        = false;
    predicate_err = p101_error_create(false);

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
    FILE *stream;
    char  line[READ_BUF_LEN];
    bool  hit;

    P101_TRACE(env);
    stream  = NULL;
    hit     = false;
    name[0] = '\0';

    if(!p101_error_path_walk_file_exists(env, path))
    {
        goto done;
    }

    stream = p101_fopen(env, err, path, "r");

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
