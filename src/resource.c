#include "resource.h"
#include "constants.h"
#include <errno.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary);
static bool parse_json_size(const struct p101_env *env, const char *text, const char *key, size_t *value);

void p101_error_path_walk_read_resource_json(const struct p101_env *env, struct p101_error *err, const char *path, struct resource_summary *summary)
{
    FILE  *stream;
    char   buffer[TRACKER_OUTPUT_LIMIT];
    size_t used;

    P101_TRACE(env);
    p101_memset(env, summary, 0, sizeof(*summary));
    stream = p101_fopen(env, err, path, "r");
    used   = 0;

    if(stream == NULL)
    {
        goto done;
    }

    while(p101_error_has_no_error(err) && used < sizeof(buffer) - 1U)
    {
        const char *line;

        line = p101_fgets(env, err, buffer + used, (int)(sizeof(buffer) - used), stream);

        if(line == NULL)
        {
            break;
        }

        used = p101_strlen(env, buffer);
    }

    buffer[used] = '\0';
    (void)parse_resource_summary(env, buffer, summary);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static bool parse_resource_summary(const struct p101_env *env, const char *text, struct resource_summary *summary)
{
    bool parsed;
    bool records_parsed;
    bool fd_leaks_parsed;
    bool allocation_leaks_parsed;
    bool bad_releases_parsed;
    bool exec_inheritances_parsed;

    records_parsed           = parse_json_size(env, text, JSON_RECORDS, &summary->records);
    fd_leaks_parsed          = parse_json_size(env, text, JSON_FD_LEAKS, &summary->fd_leaks);
    allocation_leaks_parsed  = parse_json_size(env, text, JSON_ALLOCATION_LEAKS, &summary->allocation_leaks);
    bad_releases_parsed      = parse_json_size(env, text, JSON_BAD_RELEASES, &summary->bad_releases);
    exec_inheritances_parsed = parse_json_size(env, text, JSON_EXEC_INHERITANCES, &summary->exec_inheritances);
    if(!exec_inheritances_parsed)
    {
        summary->exec_inheritances = 0U;
    }
    parsed          = (records_parsed && fd_leaks_parsed && allocation_leaks_parsed && bad_releases_parsed) != 0;
    summary->parsed = parsed;

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

    *value = parsed;
    ok     = true;

done:
    return ok;
}
