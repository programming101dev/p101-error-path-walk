#include "cli.h"
#include "constants.h"
#include "errors.h"
#include "paths.h"
#include "printer.h"
#include "resource.h"
#include "result.h"
#include "runner.h"
#include "test_hooks.h"
#include "unity.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>
#include <p101_io/p101_aio.h>
#include <p101_io/p101_fcntl.h>
#include <p101_io/p101_poll.h>
#include <p101_io/p101_stdio.h>
#include <p101_io/p101_unistd.h>
#include <p101_io/sys/p101_select.h>
#include <p101_io/sys/p101_uio.h>
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
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void p101_error_path_walk_test_set_policy_output_limit(size_t limit);

static struct p101_error *error;
static struct p101_env   *env;

struct fault_state
{
    const char *call_name;
    size_t      fail_at;
    size_t      matches;
};

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void reset_getopt(void)
{
#ifdef __GLIBC__
    optind = 0;
#else
    extern int optreset;
    optreset = 1;
    optind   = 1;
#endif
}

static int inject_selected_failure(const struct p101_env *unused_env, const char *call_name, void *user_data)
{
    struct fault_state *state;

    state = (struct fault_state *)user_data;
    if(state->call_name == NULL || p101_strcmp(unused_env, state->call_name, call_name) == 0)
    {
        state->matches++;
        if(state->matches == state->fail_at)
        {
            return EIO;
        }
    }
    return 0;
}

static void test_parse_accepts_command_after_options(void)
{
    char            *argv[] = {"p101-error-path-walk", "-n", "3", "-l", "walk", "-U", "p101-run.py", "-O", "p101-observe", "-Y", "p101-analyze.py", "-B", "p101-event-model", "-E", "12", "-F", "p101_open", "--", "prog", "arg", NULL};
    struct arguments args;

    reset_getopt();
    p101_error_path_walk_arguments_init(env, &args);

    p101_error_path_walk_parse_arguments(env, error, 20, argv, &args);
    p101_error_path_walk_check_arguments(env, error, &args);
    p101_error_path_walk_convert_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT(3U, args.max_failures);
    TEST_ASSERT_EQUAL_INT(12, args.fault_errno);
    TEST_ASSERT_EQUAL_STRING("walk", args.log_prefix);
    TEST_ASSERT_EQUAL_STRING("p101-run.py", args.p101_run);
    TEST_ASSERT_EQUAL_STRING("p101-observe", args.p101_observe);
    TEST_ASSERT_EQUAL_STRING("p101-analyze.py", args.p101_analyze);
    TEST_ASSERT_EQUAL_STRING("p101-event-model", args.event_model);
    TEST_ASSERT_EQUAL_STRING("p101_open", args.fault_name);
    TEST_ASSERT_EQUAL_STRING("prog", args.command_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg", args.command_argv[1]);
}

static void test_parse_accepts_short_io_and_repeat(void)
{
    char            *argv[] = {"p101-error-path-walk", "-F", "p101_read", "-M", "short", "-A", "7", "-R", "3", "--", "prog", NULL};
    struct arguments args;

    reset_getopt();
    p101_error_path_walk_arguments_init(env, &args);

    p101_error_path_walk_parse_arguments(env, error, 11, argv, &args);
    p101_error_path_walk_check_arguments(env, error, &args);
    p101_error_path_walk_convert_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("short", args.fault_mode);
    TEST_ASSERT_EQUAL_UINT(7U, args.fault_amount);
    TEST_ASSERT_EQUAL_UINT(3U, args.fault_repeat);
}

static void test_short_io_requires_supported_wrapper_filter(void)
{
    char            *argv[] = {"p101-error-path-walk", "-M", "short", "--", "prog", NULL};
    struct arguments args;

    reset_getopt();
    p101_error_path_walk_arguments_init(env, &args);

    p101_error_path_walk_parse_arguments(env, error, 5, argv, &args);
    p101_error_path_walk_check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_parse_rejects_missing_command(void)
{
    char            *argv[] = {"p101-error-path-walk", "-n", "0", NULL};
    struct arguments args;

    reset_getopt();
    p101_error_path_walk_arguments_init(env, &args);

    p101_error_path_walk_parse_arguments(env, error, 3, argv, &args);
    p101_error_path_walk_check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_argument_validation_covers_null_empty_modes_and_short_names(void)
{
    static const char *const modes[]       = {"error", "eintr", "timeout", "short", "uncertain"};
    static const char *const short_names[] = {"p101_read", "p101_write", "p101_pread", "p101_pwrite"};
    char                    *command[]     = {"true", NULL};
    struct arguments         args;

    p101_error_path_walk_arguments_init(env, &args);
    args.command_argv = NULL;
    p101_error_path_walk_check_arguments(env, error, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(error));

    p101_error_reset(error);
    p101_error_path_walk_arguments_init(env, &args);
    args.command_argv = command;
    args.log_prefix   = "";
    p101_error_path_walk_check_arguments(env, error, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(error));

    for(size_t index = 0U; index < 4U; index++)
    {
        const char **field;

        p101_error_reset(error);
        p101_error_path_walk_arguments_init(env, &args);
        args.command_argv = command;
        switch(index)
        {
            case 0:
                field = &args.p101_run;
                break;
            case 1:
                field = &args.p101_observe;
                break;
            case 2:
                field = &args.p101_analyze;
                break;
            default:
                field = &args.event_model;
                break;
        }
        *field = NULL;
        p101_error_path_walk_check_arguments(env, error, &args);
        TEST_ASSERT_TRUE(p101_error_has_error(error));

        p101_error_reset(error);
        p101_error_path_walk_arguments_init(env, &args);
        args.command_argv = command;
        switch(index)
        {
            case 0:
                args.p101_run = "";
                break;
            case 1:
                args.p101_observe = "";
                break;
            case 2:
                args.p101_analyze = "";
                break;
            default:
                args.event_model = "";
                break;
        }
        p101_error_path_walk_check_arguments(env, error, &args);
        TEST_ASSERT_TRUE(p101_error_has_error(error));
    }

    p101_error_reset(error);
    p101_error_path_walk_arguments_init(env, &args);
    args.command_argv = command;
    args.fault_name   = "";
    p101_error_path_walk_check_arguments(env, error, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(error));

    p101_error_reset(error);
    p101_error_path_walk_arguments_init(env, &args);
    args.command_argv = command;
    args.fault_mode   = NULL;
    p101_error_path_walk_check_arguments(env, error, &args);
    TEST_ASSERT_TRUE(p101_error_has_error(error));

    for(size_t index = 0U; index < sizeof(modes) / sizeof(modes[0]); index++)
    {
        p101_error_reset(error);
        p101_error_path_walk_arguments_init(env, &args);
        args.command_argv = command;
        args.fault_mode   = modes[index];
        args.fault_name   = (p101_strcmp(env, modes[index], "short") == 0 || p101_strcmp(env, modes[index], "uncertain") == 0) ? "p101_read" : NULL;
        p101_error_path_walk_check_arguments(env, error, &args);
        TEST_ASSERT_FALSE(p101_error_has_error(error));
    }

    for(size_t index = 0U; index < sizeof(short_names) / sizeof(short_names[0]); index++)
    {
        p101_error_reset(error);
        p101_error_path_walk_arguments_init(env, &args);
        args.command_argv = command;
        args.fault_mode   = "short";
        args.fault_name   = short_names[index];
        p101_error_path_walk_check_arguments(env, error, &args);
        TEST_ASSERT_FALSE(p101_error_has_error(error));

        p101_error_reset(error);
        args.fault_mode = "uncertain";
        p101_error_path_walk_check_arguments(env, error, &args);
        TEST_ASSERT_FALSE(p101_error_has_error(error));
    }

    p101_error_reset(error);
    p101_error_path_walk_arguments_init(env, &args);
    p101_error_path_walk_test_handle_option(env, error, &args, 99);
    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_file_exists_checks_real_files(void)
{
    TEST_ASSERT_TRUE(p101_error_path_walk_file_exists(env, __FILE__));
    TEST_ASSERT_FALSE(p101_error_path_walk_file_exists(env, "/tmp/p101-error-path-walk-definitely-missing-file"));
    p101_error_path_walk_test_force_error_create_failure(true);
    TEST_ASSERT_FALSE(p101_error_path_walk_file_exists(env, __FILE__));
    {
        char name[NAME_LEN];

        TEST_ASSERT_FALSE(p101_error_path_walk_read_fault_hit(env, error, __FILE__, name));
        TEST_ASSERT_TRUE(p101_error_has_error(error));
        p101_error_reset(error);
    }
    p101_error_path_walk_test_force_error_create_failure(false);
}

static void test_resource_policy_summary(void)
{
    static const char     json[] = "{\"schema\":\"p101-resource-policy-findings-v1\",\"findings\":[{\"id\":\"P101-FD-001\"}],\"summary\":{\"records\":3,\"processes\":1,\"findings\":1,\"process_metrics\":[]}}\n";
    struct policy_summary summary;
    FILE                 *stream;
    char                  path[] = "/tmp/p101-error-path-walk-resource-XXXXXX";
    int                   fd;

    fd = p101_mkstemp(env, error, path);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NOT_EQUAL(-1, fd);

    stream = p101_fdopen(env, error, fd, "w");
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_NOT_EQUAL(EOF, p101_fputs(env, error, json, stream));
    TEST_ASSERT_EQUAL_INT(0, p101_fclose(env, error, stream));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    p101_error_path_walk_read_policy_json(env, error, path, RESOURCE_POLICY_SCHEMA, &summary);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_TRUE(summary.parsed);
    TEST_ASSERT_EQUAL_UINT(3U, summary.records);
    TEST_ASSERT_EQUAL_UINT(1U, summary.findings);

    TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
}

static void write_text_file(char path[PATH_LEN], const char *text)
{
    FILE *stream;
    int   fd;

    p101_strncpy(env, path, "/tmp/p101-error-path-walk-test-XXXXXX", PATH_LEN);
    path[PATH_LEN - 1U] = '\0';
    fd                  = p101_mkstemp(env, error, path);
    TEST_ASSERT_NOT_EQUAL(-1, fd);
    stream = p101_fdopen(env, error, fd, "w");
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_NOT_EQUAL(EOF, p101_fputs(env, error, text, stream));
    TEST_ASSERT_EQUAL_INT(0, p101_fclose(env, error, stream));
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

static void test_paths_cover_baseline_fault_custom_and_overflow(void)
{
    struct arguments   args;
    struct run_result  result;
    struct fault_state fault;
    char              *long_prefix;

    p101_error_path_walk_arguments_init(env, &args);
    p101_error_path_walk_make_log_paths(env, error, &args, 0U, &result);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NOT_NULL(strstr(result.run_dir, "baseline.run"));
    TEST_ASSERT_NOT_NULL(strstr(result.capture_dir, "/capture"));
    TEST_ASSERT_NOT_NULL(strstr(result.analysis_dir, "/analysis"));
    TEST_ASSERT_NOT_NULL(strstr(result.resource_log_path, "/resources.log"));

    args.log_prefix = "/tmp/custom-walk";
    p101_error_path_walk_make_log_paths(env, error, &args, 9U, &result);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_NOT_NULL(strstr(result.run_dir, "fault-9.run"));

    long_prefix = (char *)p101_malloc(env, error, PATH_LEN + 64U);
    TEST_ASSERT_NOT_NULL(long_prefix);
    p101_memset(env, long_prefix, 'x', PATH_LEN + 63U);
    long_prefix[PATH_LEN + 63U] = '\0';
    args.log_prefix             = long_prefix;
    p101_error_path_walk_make_log_paths(env, error, &args, 0U, &result);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_free(env, long_prefix);

    p101_error_reset(error);
    p101_error_path_walk_arguments_init(env, &args);
    fault.call_name = "snprintf";
    fault.fail_at   = 4U;
    fault.matches   = 0U;
    p101_env_set_fault_injector(env, inject_selected_failure, &fault);
    p101_error_path_walk_make_log_paths(env, error, &args, 0U, &result);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_env_set_fault_injector(env, NULL, NULL);
}

static void test_fault_log_parser_covers_supported_and_invalid_records(void)
{
    static const char *const valid_records[] = {
        "noise\nP101FAULT\t3\t1\t2\topen\t5\terror\t1\tbefore-call\tretry-safe\n",
        "P101FAULT\t3\t1\t2\topen\t5\terror\t1\tbefore-call\tretry-safe",
        "P101FAULT\t3\t1\t2\tread\t5\tshort\t1\tafter-partial-progress\tprogress-known\r\n",
        "P101FAULT\t3\t1\t2\twrite\t110\tuncertain\t1\tafter-dispatch\toutcome-uncertain\n",
    };
    static const char *const invalid_records[] = {
        "P101FAULT\n",
        "P101FAULT\t3\n",
        "P101FAULT\t3\t1\n",
        "P101FAULT\t3\t1\t2\n",
        "P101FAULT\t3\t1\t2\topen\n",
        "P101FAULT\t3\t1\t2\tread\t5\n",
        "P101FAULT\t3\t1\t2\tread\t5\tshort\n",
        "P101FAULT\t3\t1\t2\tread\t5\tshort\t1\n",
        "P101FAULT\t3\t1\t2\tread\t5\tshort\t1\tafter-partial-progress\n",
        "P101FAULT\t3\t1\t2\tread\t5\tshort\t1\tafter-partial-progress\tprogress-known\textra\n",
        "P101FAULT\t3\t1\t2\tread\t5\tshort\t1\tbefore-call\tretry-safe\n",
        "P101FAULT\t3\t1\t2\twrite\t5\tunknown\t1\tbefore-call\tretry-safe\n",
        "P101FAULT\t2\t1\t2\tread\t5\tshort\t1\n",
    };
    char path[PATH_LEN];
    char name[NAME_LEN];

    TEST_ASSERT_FALSE(p101_error_path_walk_read_fault_hit(env, error, "/tmp/p101-error-path-walk-no-fault-log", name));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    write_text_file(path, "noise only\n");
    TEST_ASSERT_FALSE(p101_error_path_walk_read_fault_hit(env, error, path, name));
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));

    for(size_t index = 0U; index < sizeof(valid_records) / sizeof(valid_records[0]); index++)
    {
        write_text_file(path, valid_records[index]);
        TEST_ASSERT_TRUE(p101_error_path_walk_read_fault_hit(env, error, path, name));
        TEST_ASSERT_FALSE(p101_error_has_error(error));
        TEST_ASSERT_NOT_EQUAL('\0', name[0]);
        TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
    }

    for(size_t index = 0U; index < sizeof(invalid_records) / sizeof(invalid_records[0]); index++)
    {
        write_text_file(path, invalid_records[index]);
        TEST_ASSERT_FALSE(p101_error_path_walk_read_fault_hit(env, error, path, name));
        TEST_ASSERT_TRUE(p101_error_has_error(error));
        p101_error_reset(error);
        TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
    }

    {
        char *overlong;

        overlong = (char *)p101_malloc(env, error, READ_BUF_LEN + 32U);
        TEST_ASSERT_NOT_NULL(overlong);
        p101_memset(env, overlong, 'x', READ_BUF_LEN + 31U);
        overlong[READ_BUF_LEN + 31U] = '\0';
        write_text_file(path, overlong);
        TEST_ASSERT_FALSE(p101_error_path_walk_read_fault_hit(env, error, path, name));
        TEST_ASSERT_TRUE(p101_error_has_error(error));
        p101_error_reset(error);
        TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
        p101_free(env, overlong);
    }

    {
        char *full_line;

        full_line = (char *)p101_malloc(env, error, READ_BUF_LEN);
        TEST_ASSERT_NOT_NULL(full_line);
        p101_memset(env, full_line, 'x', READ_BUF_LEN - 2U);
        full_line[READ_BUF_LEN - 2U] = '\n';
        full_line[READ_BUF_LEN - 1U] = '\0';
        write_text_file(path, full_line);
        TEST_ASSERT_FALSE(p101_error_path_walk_read_fault_hit(env, error, path, name));
        TEST_ASSERT_FALSE(p101_error_has_error(error));
        TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
        p101_free(env, full_line);
    }
}

static void test_printer_covers_all_status_and_result_shapes(void)
{
    struct run_result result;

    p101_memset(env, &result, 0, sizeof(result));
    TEST_ASSERT_TRUE(p101_error_path_walk_status_is_success(0));
    TEST_ASSERT_FALSE(p101_error_path_walk_status_is_success(1 << 8));
    TEST_ASSERT_FALSE(p101_error_path_walk_status_is_success(SIGTERM));
    p101_error_path_walk_print_status_text(env, error, 0);
    p101_error_path_walk_print_status_text(env, error, SIGTERM);
    p101_error_path_walk_print_status_text(env, error, SIGSEGV | 0x80);

    result.fault_index = 0U;
    result.status      = 0;
    p101_error_path_walk_print_run_result(env, error, &result);

    result.fault_index = 1U;
    result.fault_hit   = false;
    p101_error_path_walk_print_run_result(env, error, &result);

    result.resource_log_present = true;
    result.resources.parsed     = false;
    p101_error_path_walk_print_run_result(env, error, &result);

    result.fault_hit            = true;
    result.fault_name[0]        = '\0';
    result.resource_log_present = true;
    result.resources.parsed     = true;
    result.resources.records    = 6U;
    result.resources.findings   = 6U;
    result.analysis.parsed      = true;
    result.analysis.findings    = 8U;
    p101_error_path_walk_print_run_result(env, error, &result);
    p101_strncpy(env, result.fault_name, "read", sizeof(result.fault_name));
    p101_error_path_walk_print_run_result(env, error, &result);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

static void test_policy_reader_handles_missing_and_growth(void)
{
    struct policy_summary summary;
    char                  path[PATH_LEN];
    char                 *large;

    p101_error_path_walk_read_policy_json(env, error, "/tmp/p101-error-path-walk-missing-summary", RESOURCE_POLICY_SCHEMA, &summary);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    TEST_ASSERT_FALSE(summary.parsed);
    p101_error_reset(error);

    large = (char *)p101_malloc(env, error, POLICY_INITIAL_CAPACITY + 32U);
    TEST_ASSERT_NOT_NULL(large);
    p101_memset(env, large, 'x', POLICY_INITIAL_CAPACITY + 31U);
    large[POLICY_INITIAL_CAPACITY + 31U] = '\0';
    write_text_file(path, large);
    p101_error_path_walk_read_policy_json(env, error, path, RESOURCE_POLICY_SCHEMA, &summary);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_FALSE(summary.parsed);
    TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
    write_text_file(path, large);
    p101_error_path_walk_test_set_policy_output_limit(POLICY_INITIAL_CAPACITY + 4U);
    p101_error_path_walk_read_policy_json(env, error, path, RESOURCE_POLICY_SCHEMA, &summary);
    p101_error_path_walk_test_set_policy_output_limit(POLICY_OUTPUT_LIMIT);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_FALSE(summary.parsed);
    TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
    p101_free(env, large);
}

static void expect_policy_reader_allocation_failure(const char *path, const char *wrapper)
{
    struct p101_error    *fault_error;
    struct p101_env      *fault_env;
    struct policy_summary summary;

    p101_setenv(env, error, "P101_FAULT_CALL", "1", 1);
    p101_setenv(env, error, "P101_FAULT_NAME", wrapper, 1);
    p101_setenv(env, error, "P101_FAULT_ERRNO", "12", 1);
    fault_error = p101_error_create(false);
    fault_env   = p101_env_create(fault_error, NULL);
    p101_error_path_walk_read_policy_json(fault_env, fault_error, path, RESOURCE_POLICY_SCHEMA, &summary);
    TEST_ASSERT_TRUE(p101_error_has_error(fault_error));
    TEST_ASSERT_FALSE(summary.parsed);
    p101_env_destroy(fault_env);
    p101_error_destroy(fault_error);
    p101_unsetenv(env, error, "P101_FAULT_CALL");
    p101_unsetenv(env, error, "P101_FAULT_NAME");
    p101_unsetenv(env, error, "P101_FAULT_ERRNO");
}

static void test_policy_reader_allocation_failures(void)
{
    char  path[PATH_LEN];
    char *large;

    write_text_file(path, "{}");
    expect_policy_reader_allocation_failure(path, "malloc");
    TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));

    large = (char *)p101_malloc(env, error, POLICY_INITIAL_CAPACITY + 32U);
    TEST_ASSERT_NOT_NULL(large);
    p101_memset(env, large, 'x', POLICY_INITIAL_CAPACITY + 31U);
    large[POLICY_INITIAL_CAPACITY + 31U] = '\0';
    write_text_file(path, large);
    expect_policy_reader_allocation_failure(path, "realloc");
    TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, path));
    p101_free(env, large);
}

static void init_runner_arguments(struct arguments *args, char *const command[])
{
    p101_error_path_walk_arguments_init(env, args);
    args->p101_run     = "/usr/bin/true";
    args->p101_observe = "observe";
    args->p101_analyze = "analyze";
    args->event_model  = "model";
    args->log_prefix   = "/tmp/p101-error-path-walk-unit";
    args->command_argv = command;
}

static void test_runner_helpers_cover_status_resource_and_group_models(void)
{
    struct run_result result;

    p101_memset(env, &result, 0, sizeof(result));
    TEST_ASSERT_TRUE(p101_error_path_walk_test_observe_status(0));
    TEST_ASSERT_TRUE(p101_error_path_walk_test_observe_status(EXIT_FINDINGS << 8));
    TEST_ASSERT_FALSE(p101_error_path_walk_test_observe_status(EXIT_TROUBLE << 8));
    TEST_ASSERT_FALSE(p101_error_path_walk_test_observe_status(SIGTERM));

    TEST_ASSERT_EQUAL_UINT(0U, p101_error_path_walk_test_analysis_finding_count(&result));
    TEST_ASSERT_TRUE(p101_error_path_walk_test_analysis_summary_unavailable(&result));
    result.resource_log_present = true;
    result.resources.parsed     = true;
    TEST_ASSERT_TRUE(p101_error_path_walk_test_analysis_summary_unavailable(&result));
    result.resources.has_records = true;
    result.analysis.parsed       = true;
    result.analysis.findings     = 21U;
    TEST_ASSERT_EQUAL_UINT(21U, p101_error_path_walk_test_analysis_finding_count(&result));
    TEST_ASSERT_FALSE(p101_error_path_walk_test_analysis_summary_unavailable(&result));
    result.resource_log_present = false;
    TEST_ASSERT_TRUE(p101_error_path_walk_test_analysis_summary_unavailable(&result));
    result.resource_log_present = true;
    result.resources.parsed     = false;
    TEST_ASSERT_TRUE(p101_error_path_walk_test_analysis_summary_unavailable(&result));
    result.resources.parsed      = true;
    result.resources.has_records = false;
    TEST_ASSERT_TRUE(p101_error_path_walk_test_analysis_summary_unavailable(&result));
    result.resources.has_records = true;
    result.analysis.parsed       = false;
    TEST_ASSERT_TRUE(p101_error_path_walk_test_analysis_summary_unavailable(&result));

    p101_error_path_walk_test_exercise_fault_groups(env, error);
    TEST_ASSERT_FALSE(p101_error_has_error(error));
}

static void test_run_observe_covers_argument_flush_fork_wait_and_child_failures(void)
{
    char              *command[] = {"true", NULL};
    struct arguments   args;
    struct run_result  result;
    struct fault_state fault;
    char               stdout_path[PATH_LEN];
    char               stderr_path[PATH_LEN];

    init_runner_arguments(&args, command);
    p101_memset(env, &result, 0, sizeof(result));
    p101_snprintf(env, error, stdout_path, sizeof(stdout_path), "/tmp/p101-error-path-walk-child-%ld.out", (long)p101_getpid(env));
    p101_snprintf(env, error, stderr_path, sizeof(stderr_path), "/tmp/p101-error-path-walk-child-%ld.err", (long)p101_getpid(env));
    p101_strncpy(env, result.pipeline_stdout_path, stdout_path, sizeof(result.pipeline_stdout_path));
    p101_strncpy(env, result.pipeline_stderr_path, stderr_path, sizeof(result.pipeline_stderr_path));
    p101_strncpy(env, result.run_dir, "/tmp/p101-error-path-walk-run", sizeof(result.run_dir));

    TEST_ASSERT_EQUAL_INT(0, p101_error_path_walk_test_run_observe(env, error, &args, &result));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    {
        static char *many[MAX_TOOL_ARGS + 2U];

        for(size_t index = 0U; index < MAX_TOOL_ARGS + 1U; index++)
        {
            many[index] = "x";
        }
        many[MAX_TOOL_ARGS + 1U] = NULL;
        args.command_argv        = many;
        (void)p101_error_path_walk_test_run_observe(env, error, &args, &result);
        TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
        p101_error_reset(error);
        args.command_argv = command;
    }

    fault.call_name = "fflush";
    fault.fail_at   = 1U;
    fault.matches   = 0U;
    p101_env_set_fault_injector(env, inject_selected_failure, &fault);
    (void)p101_error_path_walk_test_run_observe(env, error, &args, &result);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    fault.call_name = "p101_fork";
    fault.fail_at   = 1U;
    fault.matches   = 0U;
    (void)p101_error_path_walk_test_run_observe(env, error, &args, &result);
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    p101_env_set_fault_injector(env, NULL, NULL);
    args.p101_run = "/tmp/p101-error-path-walk-no-such-runner";
    TEST_ASSERT_EQUAL_INT(EXEC_FAILURE << 8, p101_error_path_walk_test_run_observe(env, error, &args, &result));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    args.p101_run   = "/usr/bin/true";
    fault.call_name = "p101_open";
    fault.fail_at   = 1U;
    fault.matches   = 0U;
    p101_env_set_fault_injector(env, inject_selected_failure, &fault);
    TEST_ASSERT_EQUAL_INT(EXEC_FAILURE << 8, p101_error_path_walk_test_run_observe(env, error, &args, &result));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    fault.fail_at = 2U;
    fault.matches = 0U;
    TEST_ASSERT_EQUAL_INT(EXEC_FAILURE << 8, p101_error_path_walk_test_run_observe(env, error, &args, &result));
    TEST_ASSERT_FALSE(p101_error_has_error(error));

    p101_env_set_fault_injector(env, NULL, NULL);
    fault.call_name = "p101_waitpid";
    fault.fail_at   = 1U;
    fault.matches   = 0U;
    p101_env_set_fault_injector(env, inject_selected_failure, &fault);
    (void)p101_error_path_walk_test_run_observe(env, error, &args, &result);
    TEST_ASSERT_TRUE(p101_error_has_error(error));

    p101_env_set_fault_injector(env, NULL, NULL);
    p101_error_reset(error);
    (void)p101_unlink(env, error, stdout_path);
    if(p101_error_has_error(error))
    {
        p101_error_reset(error);
    }
    (void)p101_unlink(env, error, stderr_path);
    if(p101_error_has_error(error))
    {
        p101_error_reset(error);
    }
}

static void test_run_one_case_and_run_cover_error_boundaries(void)
{
    char              *command[] = {"true", NULL};
    struct arguments   args;
    struct run_result  result;
    struct fault_state fault;
    char              *long_prefix;

    init_runner_arguments(&args, command);
    long_prefix = (char *)p101_malloc(env, error, PATH_LEN + 64U);
    TEST_ASSERT_NOT_NULL(long_prefix);
    p101_memset(env, long_prefix, 'x', PATH_LEN + 63U);
    long_prefix[PATH_LEN + 63U] = '\0';
    args.log_prefix             = long_prefix;
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_test_run_one_case(env, error, &args, 0U, &result));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    p101_free(env, long_prefix);

    init_runner_arguments(&args, command);
    long_prefix = (char *)p101_malloc(env, error, PATH_LEN + 64U);
    TEST_ASSERT_NOT_NULL(long_prefix);
    p101_memset(env, long_prefix, 'x', PATH_LEN + 63U);
    long_prefix[PATH_LEN + 63U] = '\0';
    args.log_prefix             = long_prefix;
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_run(env, error, &args));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);
    p101_free(env, long_prefix);

    init_runner_arguments(&args, command);
    fault.call_name = "p101_unsetenv";
    fault.fail_at   = 1U;
    fault.matches   = 0U;
    p101_env_set_fault_injector(env, inject_selected_failure, &fault);
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_test_run_one_case(env, error, &args, 0U, &result));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    fault.call_name = "p101_setenv";
    fault.fail_at   = 1U;
    fault.matches   = 0U;
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_test_run_one_case(env, error, &args, 0U, &result));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    fault.call_name = "p101_unsetenv";
    fault.fail_at   = 18U;
    fault.matches   = 0U;
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_test_run_one_case(env, error, &args, 0U, &result));
    TEST_ASSERT_TRUE(p101_error_has_error(error));
    p101_error_reset(error);

    fault.call_name   = "p101_setenv";
    fault.fail_at     = 5U;
    fault.matches     = 0U;
    args.max_failures = 1U;
    TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_run(env, error, &args));
    TEST_ASSERT_TRUE(p101_error_has_error(error));

    p101_env_set_fault_injector(env, NULL, NULL);
    p101_error_reset(error);

    {
        char unique_prefix[PATH_LEN];
        char invalid_path[PATH_LEN];

        init_runner_arguments(&args, command);
        p101_snprintf(env, error, unique_prefix, sizeof(unique_prefix), "/tmp/p101-error-path-walk-malformed-%ld", (long)p101_getpid(env));
        args.log_prefix = unique_prefix;
        p101_error_path_walk_make_log_paths(env, error, &args, 0U, &result);
        TEST_ASSERT_EQUAL_INT(0, p101_mkdir(env, error, result.run_dir, 0700));
        TEST_ASSERT_EQUAL_INT(0, p101_mkdir(env, error, result.capture_dir, 0700));
        write_text_file(invalid_path, "P101FAULT\t2\tbad\n");
        TEST_ASSERT_EQUAL_INT(0, p101_rename(env, error, invalid_path, result.fault_log_path));
        TEST_ASSERT_EQUAL_INT(EXIT_TROUBLE, p101_error_path_walk_test_run_one_case(env, error, &args, 0U, &result));
        TEST_ASSERT_TRUE(p101_error_has_error(error));
        p101_error_reset(error);
        TEST_ASSERT_EQUAL_INT(0, p101_unlink(env, error, result.fault_log_path));
        TEST_ASSERT_EQUAL_INT(0, p101_rmdir(env, error, result.capture_dir));
        TEST_ASSERT_EQUAL_INT(0, p101_rmdir(env, error, result.run_dir));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_command_after_options);
    RUN_TEST(test_parse_accepts_short_io_and_repeat);
    RUN_TEST(test_short_io_requires_supported_wrapper_filter);
    RUN_TEST(test_parse_rejects_missing_command);
    RUN_TEST(test_argument_validation_covers_null_empty_modes_and_short_names);
    RUN_TEST(test_file_exists_checks_real_files);
    RUN_TEST(test_resource_policy_summary);
    RUN_TEST(test_paths_cover_baseline_fault_custom_and_overflow);
    RUN_TEST(test_fault_log_parser_covers_supported_and_invalid_records);
    RUN_TEST(test_printer_covers_all_status_and_result_shapes);
    RUN_TEST(test_policy_reader_handles_missing_and_growth);
    RUN_TEST(test_policy_reader_allocation_failures);
    RUN_TEST(test_runner_helpers_cover_status_resource_and_group_models);
    RUN_TEST(test_run_observe_covers_argument_flush_fork_wait_and_child_failures);
    RUN_TEST(test_run_one_case_and_run_cover_error_boundaries);
    return UNITY_END();
}
