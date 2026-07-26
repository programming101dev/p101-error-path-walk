#define main p101_test_unused_main
#include "../src/main.c"
#undef main

#include "unity.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <errno.h>
#include <stdbool.h>

static struct p101_error *error;
static struct p101_env   *env;

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

static void test_parse_accepts_command_after_options(void)
{
    char            *argv[] = {"p101-error-path-walk", "-n", "3", "-l", "walk", "-r", "p101-resource-tracker", "-p", "p101-report", "-E", "12", "-F", "open", "--", "prog", "arg", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    args.max_failures     = DEFAULT_MAX_FAILURES;
    args.resource_tracker = DEFAULT_TRACKER_PATH;
    args.p101_report      = DEFAULT_REPORT_PATH;
    args.fault_errno      = EIO;

    parse_arguments(env, error, 18, argv, &args);
    check_arguments(env, error, &args);
    convert_arguments(env, error, &args);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_UINT(3U, args.max_failures);
    TEST_ASSERT_EQUAL_INT(12, args.fault_errno);
    TEST_ASSERT_EQUAL_STRING("walk", args.log_prefix);
    TEST_ASSERT_EQUAL_STRING("p101-resource-tracker", args.resource_tracker);
    TEST_ASSERT_EQUAL_STRING("p101-report", args.p101_report);
    TEST_ASSERT_EQUAL_STRING("open", args.fault_name);
    TEST_ASSERT_EQUAL_STRING("prog", args.command_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg", args.command_argv[1]);
}

static void test_parse_rejects_missing_command(void)
{
    char            *argv[] = {"p101-error-path-walk", "-n", "0", NULL};
    struct arguments args;

    reset_getopt();
    p101_memset(env, &args, 0, sizeof(args));
    args.max_failures     = DEFAULT_MAX_FAILURES;
    args.resource_tracker = DEFAULT_TRACKER_PATH;
    args.p101_report      = DEFAULT_REPORT_PATH;
    args.fault_errno      = EIO;

    parse_arguments(env, error, 3, argv, &args);
    check_arguments(env, error, &args);

    TEST_ASSERT_TRUE(p101_error_is_error(error, P101_ERROR_USER, ERR_USAGE));
}

static void test_file_exists_checks_real_files(void)
{
    TEST_ASSERT_TRUE(file_exists(env, __FILE__));
    TEST_ASSERT_FALSE(file_exists(env, "/tmp/p101-error-path-walk-definitely-missing-file"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_accepts_command_after_options);
    RUN_TEST(test_parse_rejects_missing_command);
    RUN_TEST(test_file_exists_checks_real_files);
    return UNITY_END();
}
