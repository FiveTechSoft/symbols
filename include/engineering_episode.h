#ifndef ENGINEERING_EPISODE_H
#define ENGINEERING_EPISODE_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define EE_MAX_RECORDS 256
#define EE_STR 96
#define EE_MAX_IMAGE (1024 * 1024)
/* All identifiers and digests are opaque, bounded, and may not contain tabs,
   newlines, or control bytes. No raw task text or diagnostic bytes. */
typedef struct {
    char episode_id[EE_STR], run_id[EE_STR], attempt_id[EE_STR];
    char parent_id[EE_STR], repo_sha[EE_STR], workspace_before[EE_STR], workspace_after[EE_STR];
    char task_signature[EE_STR], goal_provenance[EE_STR], engine[EE_STR];
    char op[EE_STR], patch_hash[EE_STR], oracle[EE_STR], oracle_version[EE_STR];
    char diagnostic[EE_STR], outcome[EE_STR], rollback[EE_STR];
    uint32_t candidate_builds, probes, tool_calls;
} ENGINEERING_EPISODE;
typedef struct { ENGINEERING_EPISODE rows[EE_MAX_RECORDS]; size_t count; } EPISODE_STORE;
/* 1 valid, 0 absent, -1 corrupt/unsupported/I/O error. Entire image is
   rejected on a bad row. Reads do not create or modify any file. */
int EpisodeLoad(const char *path, EPISODE_STORE *out);
/* 1 persisted, 0 refused. No solver is connected to this API. */
int EpisodeAppend(const char *path, const ENGINEERING_EPISODE *row);
/* Read-only aggregate printer; same function used by the CLI and tests. */
int EpisodeReport(const char *path, FILE *out, FILE *error);
#endif
