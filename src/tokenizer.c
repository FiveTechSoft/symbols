#include "tokenizer.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

TOKENIZER *TokenizerCreate(const char *text)
{
    if (!text)
        return NULL;

    TOKENIZER *tok = (TOKENIZER *)malloc(sizeof(TOKENIZER));
    if (!tok)
        return NULL;

    tok->input = text;
    tok->pos = 0;
    tok->length = (uint32_t)strlen(text);
    return tok;
}

void TokenizerDestroy(TOKENIZER *tok)
{
    if (tok)
        free(tok);
}

static int is_word_char(char c)
{
    return isalpha((unsigned char)c) || c == '_' || c == '-';
}

static int is_punct_char(char c)
{
    return c == '.' || c == ',' || c == ';' || c == ':' ||
           c == '!' || c == '?' || c == '(' || c == ')' ||
           c == '"' || c == '\'' || c == '\n' || c == '\r';
}

TOKEN TokenizerNext(TOKENIZER *tok)
{
    TOKEN t;
    t.text = NULL;
    t.type = TOKEN_EOF;
    t.start = 0;
    t.length = 0;

    if (!tok)
        return t;

    while (tok->pos < tok->length)
    {
        char c = tok->input[tok->pos];

        if (isspace((unsigned char)c))
        {
            tok->pos++;
            continue;
        }

        t.start = tok->pos;

        if (is_punct_char(c))
        {
            t.type = TOKEN_PUNCTUATION;
            t.length = 1;
            tok->pos++;
            t.text = &tok->input[t.start];
            return t;
        }

        if (isdigit((unsigned char)c))
        {
            t.type = TOKEN_NUMBER;
            while (tok->pos < tok->length &&
                   (isdigit((unsigned char)tok->input[tok->pos]) ||
                    tok->input[tok->pos] == '.'))
            {
                tok->pos++;
            }
            t.length = tok->pos - t.start;
            t.text = &tok->input[t.start];
            return t;
        }

        if (is_word_char(c))
        {
            int all_upper = isupper((unsigned char)c);
            while (tok->pos < tok->length &&
                   is_word_char(tok->input[tok->pos]))
            {
                if (islower((unsigned char)tok->input[tok->pos]))
                    all_upper = 0;
                tok->pos++;
            }
            t.type = all_upper ? TOKEN_UPPER_WORD : TOKEN_WORD;
            t.length = tok->pos - t.start;
            t.text = &tok->input[t.start];
            return t;
        }

        tok->pos++;
    }

    return t;
}
