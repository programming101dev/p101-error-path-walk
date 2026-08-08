#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <errno.h>
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
#include <p101_record/record.h>
#include <stdio.h>
#include <string.h>

static void               join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
static bool               fault_semantics_valid(const struct p101_env *env, const char *mode, const char *phase, const char *disposition);
static struct p101_error *create_predicate_error(void);

/* The field layout of a P101FAULT record, in wire order. */
enum
{
    FAULT_FIELD_TAG = 0,
    FAULT_FIELD_VERSION,
    FAULT_FIELD_PID,
    FAULT_FIELD_CALLS_SEEN,
    FAULT_FIELD_NAME,
    FAULT_FIELD_ERRNO,
    FAULT_FIELD_MODE,
    FAULT_FIELD_AMOUNT,
    FAULT_FIELD_PHASE,
    FAULT_FIELD_DISPOSITION,
    FAULT_FIELD_COUNT
};

#ifdef P101_ERROR_PATH_WALK_TESTING
static bool force_error_create_failure;
#endif

static struct p101_error *create_predicate_error(void)
{
    struct p101_error *predicate_err;

    predicate_err = NULL;
#ifdef P101_ERROR_PATH_WALK_TESTING
    if(!force_error_create_failure)
    {
        predicate_err = p101_error_create(false);
    }
#else
    predicate_err = p101_error_create(false);
#endif

    return predicate_err;
}

void p101_error_path_walk_make_log_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, unsigned int fault_index, struct run_result *result)
{
    pid_t       p101_call_result_1;
    const char *prefix;
    long        pid_value;

    P101_TRACE_SCOPE(env);
    prefix             = (args->log_prefix == NULL) ? DEFAULT_LOG_PREFIX : args->log_prefix;
    p101_call_result_1 = p101_getpid(env);
    pid_value          = (long)p101_call_result_1;

    if(fault_index == 0)
    {
        p101_snprintf(env, err, result->run_dir, PATH_LEN, "%s-%ld-baseline.run", prefix, pid_value);
        p101_snprintf(env, err, result->pipeline_stdout_path, PATH_LEN, "%s-%ld-baseline.run.stdout.txt", prefix, pid_value);
        p101_snprintf(env, err, result->pipeline_stderr_path, PATH_LEN, "%s-%ld-baseline.run.stderr.txt", prefix, pid_value);
    }
    else
    {
        p101_snprintf(env, err, result->run_dir, PATH_LEN, "%s-%ld-fault-%u.run", prefix, pid_value, fault_index);
        p101_snprintf(env, err, result->pipeline_stdout_path, PATH_LEN, "%s-%ld-fault-%u.run.stdout.txt", prefix, pid_value, fault_index);
        p101_snprintf(env, err, result->pipeline_stderr_path, PATH_LEN, "%s-%ld-fault-%u.run.stderr.txt", prefix, pid_value, fault_index);
    }

    result->run_dir[PATH_LEN - 1]              = '\0';
    result->pipeline_stdout_path[PATH_LEN - 1] = '\0';
    result->pipeline_stderr_path[PATH_LEN - 1] = '\0';

    join_path(env, err, result->capture_dir, result->run_dir, "capture");
    join_path(env, err, result->analysis_dir, result->run_dir, "analysis");
    join_path(env, err, result->resource_log_path, result->capture_dir, "resources.log");
    join_path(env, err, result->call_log_path, result->capture_dir, "calls.log");
    join_path(env, err, result->fault_log_path, result->capture_dir, "fault.log");
    join_path(env, err, result->resource_json_path, result->analysis_dir, "resource-report.json");
    join_path(env, err, result->analysis_json_path, result->analysis_dir, "correlated-report.json");
    join_path(env, err, result->report_path, result->analysis_dir, "correlated-report.txt");
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
    exists        = false;
    predicate_err = create_predicate_error();

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
    int                p101_expression_result_8;
    const char        *p101_call_result_9;
    const char        *p101_call_result_2;
    int                p101_call_result_3;
    int                p101_call_result_4;
    bool               p101_call_result_5;
    struct p101_error *predicate_err;
    FILE              *stream;
    char               line[READ_BUF_LEN];
    bool               hit;

    P101_TRACE_SCOPE(env);
    stream        = NULL;
    hit           = false;
    name[0]       = '\0';
    predicate_err = create_predicate_error();
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
        char  *cursor;
        char  *fields[FAULT_FIELD_COUNT];
        size_t count;
        size_t index;
        size_t length;

        p101_call_result_2 = p101_fgets(env, err, line, sizeof(line), stream);
        if(p101_call_result_2 == NULL)
        {
            break;
        }
        length                   = p101_strlen(env, line);
        p101_expression_result_8 = 0;
        if(length == sizeof(line) - 1U)
        {
            p101_call_result_9 = p101_strchr(env, line, '\n');
            if(p101_call_result_9 == NULL)
            {
                p101_expression_result_8 = 1;
            }
        }
        if(p101_expression_result_8)
        {
            P101_ERROR_RAISE_USER(err, "The fault log contains an over-long record.", ERR_USAGE);
            goto done;
        }

        while(length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r'))
        {
            length--;
            line[length] = '\0';
        }

        cursor                  = line;
        fields[FAULT_FIELD_TAG] = p101_record_split(&cursor);
        p101_call_result_3      = p101_strcmp(env, fields[FAULT_FIELD_TAG], P101_ENV_FAULT_LOG_TAG);
        if(p101_call_result_3 != 0)
        {
            continue;
        }

        for(count = 1U; count < FAULT_FIELD_COUNT && cursor != NULL; count++)
        {
            fields[count] = p101_record_split(&cursor);
        }

        if(count != FAULT_FIELD_COUNT || cursor != NULL)
        {
            P101_ERROR_RAISE_USER(err, "The fault log contains a malformed P101FAULT record.", ERR_USAGE);
            goto done;
        }

        for(index = 0U; index < FAULT_FIELD_COUNT; index++)
        {
            p101_record_unescape_field(fields[index]);
        }

        p101_call_result_4 = p101_strcmp(env, fields[FAULT_FIELD_VERSION], P101_ENV_FAULT_LOG_VERSION);
        if(p101_call_result_4 != 0)
        {
            P101_ERROR_RAISE_USER(err, "The fault log version is not supported.", ERR_USAGE);
            goto done;
        }
        p101_call_result_5 = fault_semantics_valid(env, fields[FAULT_FIELD_MODE], fields[FAULT_FIELD_PHASE], fields[FAULT_FIELD_DISPOSITION]);
        if(!p101_call_result_5)
        {
            P101_ERROR_RAISE_USER(err, "The fault log contains inconsistent phase/disposition semantics.", ERR_USAGE);
            goto done;
        }

        p101_strncpy(env, name, fields[FAULT_FIELD_NAME], NAME_LEN - 1U);
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

static bool fault_semantics_valid(const struct p101_env *env, const char *mode, const char *phase, const char *disposition)
{
    bool                p101_call_result_6;
    p101_env_fault_mode parsed_mode;
    bool                valid;

    valid              = false;
    p101_call_result_6 = p101_env_fault_mode_from_name(mode, &parsed_mode);

    if(p101_call_result_6)
    {
        struct p101_env_fault_defaults defaults;
        bool                           p101_call_result_7;

        p101_call_result_7 = p101_env_fault_mode_defaults(parsed_mode, &defaults);

        if(p101_call_result_7)
        {
            int p101_call_result_8;
            int p101_call_result_9;

            p101_call_result_8 = p101_strcmp(env, phase, p101_env_fault_phase_name(defaults.phase));
            p101_call_result_9 = p101_strcmp(env, disposition, p101_env_fault_disposition_name(defaults.disposition));
            valid              = (p101_call_result_8 == 0 && p101_call_result_9 == 0) != 0;
        }
    }

    return valid;
}

#ifdef P101_ERROR_PATH_WALK_TESTING
void p101_error_path_walk_test_force_error_create_failure(bool force)
{
    force_error_create_failure = force;
}
#endif
