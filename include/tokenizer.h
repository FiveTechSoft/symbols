#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdint.h>

typedef enum
{
    TOKEN_WORD,
    TOKEN_NUMBER,
    TOKEN_PUNCTUATION,
    TOKEN_UPPER_WORD,
    TOKEN_EOF
} TOKEN_TYPE;

typedef struct
{
    const char *text;
    TOKEN_TYPE type;
    uint32_t start;
    uint32_t length;
} TOKEN;

typedef struct
{
    const char *input;
    uint32_t pos;
    uint32_t length;
} TOKENIZER;

TOKENIZER *TokenizerCreate(const char *text);
void TokenizerDestroy(TOKENIZER *tok);
TOKEN TokenizerNext(TOKENIZER *tok);

#endif
