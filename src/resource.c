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
    buffer = (char *)p101_malloc(env, err, capacity);
    if(buffer == NULL)
    {
        goto done;
    }
    buffer[0] = '\0';

    while(used < output_limit - 1U)
    {
        const char *line;

        if(used == capacity - 1U)
        {
            char  *resized;
            size_t next_capacity;

            next_capacity = capacity > output_limit / 2U ? output_limit : capacity * 2U;
            resized       = (char *)p101_realloc(env, err, buffer, next_capacity);
            if(resized == NULL)
            {
                goto done;
            }
            buffer   = resized;
            capacity = next_capacity;
        }

        line = p101_fgets(env, err, buffer + used, (int)(capacity - used), stream);

        if(line == NULL)
        {
            break;
        }

        used += p101_strlen(env, buffer + used);
    }

    buffer[used] = '\0';
    (void)p101_tool_event_parse_policy_summary_json(buffer, schema, summary);

done:
    p101_free(env, buffer);
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
