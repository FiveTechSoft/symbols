/* episodic_memory.c: Persistent, continuous episodic memory for conversational learning.
   Pure ISO C11 standard library with zero external dependencies. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "episodic_memory.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

static void TrimStr(char *str)
{
    if (!str) return;
    char *p = str;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != str) memmove(str, p, strlen(p) + 1);
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
    {
        str[--len] = '\0';
    }
}

static void ToLowerStr(char *str)
{
    if (!str) return;
    for (; *str; str++)
    {
        *str = (char)tolower((unsigned char)*str);
    }
}

static void EnsureParentDir(const char *filepath)
{
    if (!filepath) return;
    char path[512];
    strncpy(path, filepath, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';

    for (char *p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            char save = *p;
            *p = '\0';
            if (path[0] != '\0')
            {
                MKDIR(path);
            }
            *p = save;
        }
    }
}

int EpisodicStoreInit(EPISODIC_STORE *store, const char *filepath)
{
    if (!store) return 0;
    memset(store, 0, sizeof(*store));
    const char *path = (filepath && filepath[0]) ? filepath : "data/memory/episodic.tsv";
    strncpy(store->filepath, path, sizeof(store->filepath) - 1);
    store->filepath[sizeof(store->filepath) - 1] = '\0';
    store->auto_save = 1;
    store->capacity = EPISODIC_INITIAL_CAP;
    store->records = (EPISODIC_RECORD *)calloc(store->capacity, sizeof(EPISODIC_RECORD));
    return (store->records != NULL);
}

void EpisodicStoreDestroy(EPISODIC_STORE *store)
{
    if (!store) return;
    if (store->records)
    {
        free(store->records);
        store->records = NULL;
    }
    store->count = 0;
    store->capacity = 0;
}

uint32_t EpisodicStoreCount(const EPISODIC_STORE *store)
{
    return store ? store->count : 0;
}

const EPISODIC_RECORD *EpisodicStoreGet(const EPISODIC_STORE *store, uint32_t idx)
{
    if (!store || !store->records || idx >= store->count) return NULL;
    return &store->records[idx];
}

int EpisodicStoreExists(const EPISODIC_STORE *store, const char *subject, const char *relation, const char *object)
{
    if (!store || !store->records || !subject || !relation || !object) return 0;
    char s_low[EPISODIC_STR_MAX], r_low[EPISODIC_STR_MAX], o_low[EPISODIC_STR_MAX];
    strncpy(s_low, subject, sizeof(s_low) - 1); s_low[sizeof(s_low) - 1] = '\0'; ToLowerStr(s_low);
    strncpy(r_low, relation, sizeof(r_low) - 1); r_low[sizeof(r_low) - 1] = '\0'; ToLowerStr(r_low);
    strncpy(o_low, object, sizeof(o_low) - 1); o_low[sizeof(o_low) - 1] = '\0'; ToLowerStr(o_low);

    for (uint32_t i = 0; i < store->count; i++)
    {
        char cur_s[EPISODIC_STR_MAX], cur_r[EPISODIC_STR_MAX], cur_o[EPISODIC_STR_MAX];
        strncpy(cur_s, store->records[i].subject, sizeof(cur_s) - 1); cur_s[sizeof(cur_s) - 1] = '\0'; ToLowerStr(cur_s);
        strncpy(cur_r, store->records[i].relation, sizeof(cur_r) - 1); cur_r[sizeof(cur_r) - 1] = '\0'; ToLowerStr(cur_r);
        strncpy(cur_o, store->records[i].object, sizeof(cur_o) - 1); cur_o[sizeof(cur_o) - 1] = '\0'; ToLowerStr(cur_o);

        if (strcmp(s_low, cur_s) == 0 && strcmp(r_low, cur_r) == 0 && strcmp(o_low, cur_o) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static int EnsureCapacity(EPISODIC_STORE *store)
{
    if (store->count >= store->capacity)
    {
        uint32_t new_cap = store->capacity ? store->capacity * 2 : EPISODIC_INITIAL_CAP;
        EPISODIC_RECORD *new_recs = (EPISODIC_RECORD *)realloc(store->records, new_cap * sizeof(EPISODIC_RECORD));
        if (!new_recs) return 0;
        store->records = new_recs;
        store->capacity = new_cap;
    }
    return 1;
}

int EpisodicStoreAppend(EPISODIC_STORE *store, const char *subject, const char *relation, const char *object, const char *source)
{
    if (!store || !subject || !relation || !object) return 0;

    char s[EPISODIC_STR_MAX], r[EPISODIC_STR_MAX], o[EPISODIC_STR_MAX], src[EPISODIC_STR_MAX];
    strncpy(s, subject, sizeof(s) - 1); s[sizeof(s) - 1] = '\0'; TrimStr(s);
    strncpy(r, relation, sizeof(r) - 1); r[sizeof(r) - 1] = '\0'; TrimStr(r);
    strncpy(o, object, sizeof(o) - 1); o[sizeof(o) - 1] = '\0'; TrimStr(o);
    const char *src_in = (source && source[0]) ? source : "user";
    strncpy(src, src_in, sizeof(src) - 1); src[sizeof(src) - 1] = '\0'; TrimStr(src);

    if (s[0] == '\0' || r[0] == '\0' || o[0] == '\0') return 0;

    if (EpisodicStoreExists(store, s, r, o))
    {
        return 2; /* Already exists */
    }

    if (!EnsureCapacity(store)) return 0;

    EPISODIC_RECORD *rec = &store->records[store->count++];
    strncpy(rec->subject, s, sizeof(rec->subject) - 1);
    rec->subject[sizeof(rec->subject) - 1] = '\0';
    strncpy(rec->relation, r, sizeof(rec->relation) - 1);
    rec->relation[sizeof(rec->relation) - 1] = '\0';
    strncpy(rec->object, o, sizeof(rec->object) - 1);
    rec->object[sizeof(rec->object) - 1] = '\0';
    strncpy(rec->source, src, sizeof(rec->source) - 1);
    rec->source[sizeof(rec->source) - 1] = '\0';
    rec->timestamp = (uint64_t)time(NULL);

    if (store->auto_save)
    {
        if (!EpisodicStoreSave(store))
        {
            /* Roll the append back: RAM must never claim more than disk. */
            store->count--;
            return 0;
        }
    }
    return 1;
}

/* Write one full store image to a sibling temporary file and rename it
   over the target. The target is replaced in one atomic step, so a
   crash or I/O error mid-write can never leave a truncated store
   behind. Any failure removes the temporary file, leaves the previous
   target untouched and returns 0. */
static int WriteStoreAtomic(const char *filepath, const char *header,
                            const EPISODIC_RECORD *records, uint32_t count)
{
    char tmp_path[520];
    int ok = 1;

    if (!filepath || filepath[0] == '\0') return 0;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);
    tmp_path[sizeof(tmp_path) - 1] = '\0';

    EnsureParentDir(filepath);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return 0;

    if (fprintf(f, "%s", header) < 0) ok = 0;
    for (uint32_t i = 0; ok && i < count; i++)
    {
        const EPISODIC_RECORD *rec = &records[i];
        if (fprintf(f, "%s\t%s\t%s\t%s\t%llu\n",
                    rec->subject, rec->relation, rec->object,
                    rec->source[0] ? rec->source : "user",
                    (unsigned long long)rec->timestamp) < 0)
        {
            ok = 0;
        }
    }
    if (ok && fflush(f) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;

    if (!ok)
    {
        remove(tmp_path);
        return 0;
    }

#ifdef _WIN32
    if (!MoveFileExA(tmp_path, filepath,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        remove(tmp_path);
        return 0;
    }
#else
    if (rename(tmp_path, filepath) != 0)
    {
        remove(tmp_path);
        return 0;
    }
#endif
    return 1;
}

int EpisodicStoreSave(const EPISODIC_STORE *store)
{
    if (!store || store->filepath[0] == '\0') return 0;
    return WriteStoreAtomic(store->filepath,
                            "# symbols episodic memory store (TSV: subject \\t relation \\t object \\t source \\t timestamp)\n",
                            store->records, store->count);
}

int EpisodicStoreForget(EPISODIC_STORE *store, const char *subject, const char *relation, const char *object)
{
    if (!store || !store->records || !subject || !relation || !object) return 0;
    if (!EpisodicStoreExists(store, subject, relation, object)) return 2;

    char s_low[EPISODIC_STR_MAX], r_low[EPISODIC_STR_MAX], o_low[EPISODIC_STR_MAX];
    strncpy(s_low, subject, sizeof(s_low) - 1); s_low[sizeof(s_low) - 1] = '\0'; ToLowerStr(s_low);
    strncpy(r_low, relation, sizeof(r_low) - 1); r_low[sizeof(r_low) - 1] = '\0'; ToLowerStr(r_low);
    strncpy(o_low, object, sizeof(o_low) - 1); o_low[sizeof(o_low) - 1] = '\0'; ToLowerStr(o_low);

    /* Append and Load deduplicate through EpisodicStoreExists, so at
       most one record can match. */
    uint32_t pos = store->count;
    for (uint32_t i = 0; i < store->count; i++)
    {
        char cur_s[EPISODIC_STR_MAX], cur_r[EPISODIC_STR_MAX], cur_o[EPISODIC_STR_MAX];
        strncpy(cur_s, store->records[i].subject, sizeof(cur_s) - 1); cur_s[sizeof(cur_s) - 1] = '\0'; ToLowerStr(cur_s);
        strncpy(cur_r, store->records[i].relation, sizeof(cur_r) - 1); cur_r[sizeof(cur_r) - 1] = '\0'; ToLowerStr(cur_r);
        strncpy(cur_o, store->records[i].object, sizeof(cur_o) - 1); cur_o[sizeof(cur_o) - 1] = '\0'; ToLowerStr(cur_o);

        if (strcmp(s_low, cur_s) == 0 && strcmp(r_low, cur_r) == 0 && strcmp(o_low, cur_o) == 0)
        {
            pos = i;
            break;
        }
    }
    if (pos >= store->count) return 2;

    EPISODIC_RECORD removed = store->records[pos];
    memmove(&store->records[pos], &store->records[pos + 1],
            (store->count - pos - 1) * sizeof(EPISODIC_RECORD));
    store->count--;

    if (store->auto_save && !EpisodicStoreSave(store))
    {
        /* Persistence failed: restore the record so RAM matches disk. */
        if (EnsureCapacity(store))
        {
            memmove(&store->records[pos + 1], &store->records[pos],
                    (store->count - pos) * sizeof(EPISODIC_RECORD));
            store->records[pos] = removed;
            store->count++;
        }
        return 0;
    }
    return 1;
}

uint32_t EpisodicStoreLoad(EPISODIC_STORE *store)
{
    if (!store || store->filepath[0] == '\0') return 0;

    FILE *f = fopen(store->filepath, "r");
    if (!f) return 0;

    uint32_t loaded = 0;
    char line[512];
    while (fgets(line, sizeof(line), f))
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
        {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') continue;

        char *t1 = strchr(line, '\t');
        if (!t1) continue;
        *t1 = '\0';
        char *sub = line;

        char *t2 = strchr(t1 + 1, '\t');
        if (!t2) continue;
        *t2 = '\0';
        char *rel = t1 + 1;

        char *obj = t2 + 1;
        char *src = "user";
        uint64_t ts = 0;

        char *t3 = strchr(obj, '\t');
        if (t3)
        {
            *t3 = '\0';
            src = t3 + 1;
            char *t4 = strchr(src, '\t');
            if (t4)
            {
                *t4 = '\0';
                ts = (uint64_t)strtoull(t4 + 1, NULL, 10);
            }
        }

        TrimStr(sub);
        TrimStr(rel);
        TrimStr(obj);
        TrimStr(src);

        if (sub[0] == '\0' || rel[0] == '\0' || obj[0] == '\0') continue;

        if (!EpisodicStoreExists(store, sub, rel, obj))
        {
            if (!EnsureCapacity(store)) break;
            EPISODIC_RECORD *rec = &store->records[store->count++];
            strncpy(rec->subject, sub, sizeof(rec->subject) - 1);
            rec->subject[sizeof(rec->subject) - 1] = '\0';
            strncpy(rec->relation, rel, sizeof(rec->relation) - 1);
            rec->relation[sizeof(rec->relation) - 1] = '\0';
            strncpy(rec->object, obj, sizeof(rec->object) - 1);
            rec->object[sizeof(rec->object) - 1] = '\0';
            strncpy(rec->source, src, sizeof(rec->source) - 1);
            rec->source[sizeof(rec->source) - 1] = '\0';
            rec->timestamp = ts ? ts : (uint64_t)time(NULL);
            loaded++;
        }
    }

    fclose(f);
    return loaded;
}

int EpisodicStoreClear(EPISODIC_STORE *store)
{
    if (!store) return 0;
    if (store->filepath[0] != '\0')
    {
        if (!WriteStoreAtomic(store->filepath,
                              "# symbols episodic memory store (cleared)\n",
                              NULL, 0))
        {
            return 0;
        }
    }
    store->count = 0;
    return 1;
}
