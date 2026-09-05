#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <stdint.h>

typedef enum
{
    ACT_NONE = 0,
    ACT_GREETING,
    ACT_FAREWELL,
    ACT_THANKS,
    ACT_AGREEMENT,
    ACT_DISAGREEMENT,
    ACT_QUESTION,
    ACT_STATEMENT,
    ACT_APOLOGY,
    ACT_IDENTITY_QUERY,
    ACT_CAPABILITY_QUERY,
    ACT_CONFUSION
} SPEECH_ACT;

typedef struct
{
    SPEECH_ACT act;
    float      confidence;
    char       entity[128];
} DIALOGUE_INTENT;

/* Classify user input into a speech act */
DIALOGUE_INTENT DialogueClassify(const char *input);

/* Generate appropriate response for non-knowledge speech acts.
   Returns 1 if handled (response written to out), 0 if not handled. */
int DialogueRespond(SPEECH_ACT act, const char *input,
                    char *out, uint32_t out_size);

#endif
