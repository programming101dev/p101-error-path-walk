#include "resource.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdio.h>

#ifdef P101_ERROR_PATH_WALK_TESTING
static size_t test_policy_output_limit = POLICY_OUTPUT_LIMIT;

void p101_error_path_walk_test_set_policy_output_limit(size_t limit)
{
    test_policy_output_limit = limit;
}
#endif

void p101_error_path_walk_read_policy_json(const struct p101_env *env, struct p101_error *err, const char *path, const char *schema, struct policy_summary *summary)
{
    int    p101_expression_result_4;
    bool   p101_call_result_5;
    void  *p101_call_result_1;
    void  *p101_call_result_2;
    bool   p101_call_result_3;
    FILE  *stream;
    char  *buffer;
    size_t capacity;
    size_t output_limit;
    size_t used;

    P101_TRACE_SCOPE(env);
    p101_memset(env, summary, 0, sizeof(*summary));
    stream   = p101_fopen(env, err, path, "r");
    buffer   = NULL;
    capacity = POLICY_INITIAL_CAPACITY;
#ifdef P101_ERROR_PATH_WALK_TESTING
    output_limit = test_policy_output_limit;
#else
    output_limit = POLICY_OUTPUT_LIMIT;
#endif
    used = 0;

    if(stream == NULL)
    {
        goto done;
    }
    p101_call_result_1 = p101_malloc(env, err, capacity);
    buffer             = (char *)p101_call_result_1;
    if(buffer == NULL)
    {
        goto done;
    }
    buffer[0] = '\0';

    while(used < output_limit - 1U)
    {
        size_t read_count;

        if(used == capacity - 1U)
        {
            char  *resized;
            size_t next_capacity;

            next_capacity      = capacity > output_limit / 2U ? output_limit : capacity * 2U;
            p101_call_result_2 = p101_realloc(env, err, buffer, next_capacity);
            resized            = (char *)p101_call_result_2;
            if(resized == NULL)
            {
                goto done;
            }
            buffer   = resized;
            capacity = next_capacity;
        }

        read_count = p101_fread(env, err, buffer + used, 1U, capacity - used - 1U, stream);
        used += read_count;
        p101_call_result_5 = p101_error_has_error(err);
        if(p101_call_result_5)
        {
            p101_expression_result_4 = 1;
        }
        else
        {
            if(read_count == 0U)
            {
                p101_expression_result_4 = 1;
            }
            else
            {
                p101_expression_result_4 = 0;
            }
        }
        if(p101_expression_result_4)
        {
            break;
        }
    }

    buffer[used]       = '\0';
    p101_call_result_3 = p101_tool_event_parse_policy_summary_json_n(buffer, used, schema, summary);
    (void)p101_call_result_3;

done:
    p101_free(env, buffer);
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
