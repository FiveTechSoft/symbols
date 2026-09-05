#include "tokenizer.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_basic(void)
{
    TOKENIZER *tok = TokenizerCreate("El gato come pescado.");
    assert(tok != NULL);

    TOKEN t;

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);
    assert(t.length == 2);
    assert(strncmp(t.text, "El", 2) == 0);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);
    assert(t.length == 4);
    assert(strncmp(t.text, "gato", 4) == 0);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);
    assert(t.length == 4);
    assert(strncmp(t.text, "come", 4) == 0);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);
    assert(t.length == 7);
    assert(strncmp(t.text, "pescado", 7) == 0);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_PUNCTUATION);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_EOF);

    TokenizerDestroy(tok);
    printf("  PASS test_basic\n");
}

static void test_numbers(void)
{
    TOKENIZER *tok = TokenizerCreate("Tengo 42 gatos y 3.14 ratones.");
    TOKEN t;

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_NUMBER);
    assert(strncmp(t.text, "42", 2) == 0);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_WORD);

    t = TokenizerNext(tok);
    assert(t.type == TOKEN_NUMBER);
    assert(strncmp(t.text, "3.14", 4) == 0);

    TokenizerDestroy(tok);
    printf("  PASS test_numbers\n");
}

static void test_empty(void)
{
    TOKENIZER *tok = TokenizerCreate("");
    TOKEN t = TokenizerNext(tok);
    assert(t.type == TOKEN_EOF);
    TokenizerDestroy(tok);

    tok = TokenizerCreate("   ");
    t = TokenizerNext(tok);
    assert(t.type == TOKEN_EOF);
    TokenizerDestroy(tok);

    printf("  PASS test_empty\n");
}

int main(void)
{
    printf("=== test_tokenizer ===\n");
    test_basic();
    test_numbers();
    test_empty();
    printf("All tokenizer tests passed.\n");
    return 0;
}
