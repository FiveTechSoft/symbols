#ifndef ATTEMPT_CAPTURE_H
#define ATTEMPT_CAPTURE_H
#include <stddef.h>
/* Audit-only opt-in. A missing pair is unavailable, never a replayable edge.
   The caller never reads the capture to select an operator. */
typedef struct {
    char root[1024];
    char run[96];
    unsigned attempt;
    int ready;
} ATTEMPT_CAPTURE;
int AttemptCaptureBegin(ATTEMPT_CAPTURE *c,const char *workspace,const char *run,unsigned attempt);
int AttemptCaptureEnd(ATTEMPT_CAPTURE *c,const char *workspace,const char *outcome);
/* Read-only validation of every published pair in a run, including adjacency. */
int AttemptCaptureValidate(const char *capture_root,const char *run,unsigned count);
#endif
