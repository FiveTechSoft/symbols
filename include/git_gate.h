#ifndef GIT_GATE_H
#define GIT_GATE_H

#include "agent_git.h"
#include <stdio.h>

/* Command line front end for the Phase 2 Git contracts, used by a workflow:
     preflight   --expected-head SHA [--branch NAME] [--remote-sync] [--dir D]
     patch-state PATCH [--dir D]
     verify-staged PATCH [--dir D]
     verify-head   PATCH [--dir D]
   PATCH is a path relative to the directory (default "."), as AgentGitPatchState
   requires. The expected-change manifest is derived from the diff headers of the
   patch itself. Every command is read-only.

   Exit codes:
     preflight     0 ready, otherwise 10 + the GIT_PREFLIGHT_STATUS value
     patch-state   0 not applied, 3 already applied, 4 applies neither way,
                   5 check failed (unreadable, unsafe path, corrupt)
     verify-*      0 match, 1 mismatch, 2 inspection failed,
                   6 manifest could not be derived from the patch (fail closed)
     any           64 usage error
   One result line goes to out, an explanation to err. */
#define GIT_GATE_USAGE 64

#define GIT_GATE_MAX_PATHS 256
#define GIT_GATE_PATH_MAX 512

typedef struct
{
    size_t count;
    char path[GIT_GATE_MAX_PATHS][GIT_GATE_PATH_MAX];
    char status[GIT_GATE_MAX_PATHS];
} GIT_GATE_MANIFEST;

/* Derive the changed paths from the headers of a git diff file. A rename is a
   'D' for the old path plus an 'A' for the new one, a copy an 'A' for the
   destination, a mode change an 'M'. Fails closed (returns false and fills
   error) on quoted or unusual paths, more than GIT_GATE_MAX_PATHS entries, a
   repeated path, an empty patch or an unreadable file. Paths use only
   [A-Za-z0-9._/-]. */
bool GitGateDeriveManifest(const char *patch_file, GIT_GATE_MANIFEST *out,
                           char *error, size_t error_size);

int GitGateRun(int argc, char **argv, FILE *out, FILE *err);

#endif
