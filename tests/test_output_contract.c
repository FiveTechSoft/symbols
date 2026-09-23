/* test_output_contract: the output-contract reader, composer and checker
   on synthetic contracts (none is any client's real prompt). */
#include "output_contract.h"
#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;

#define CHECK(cond, msg) do { if (cond) g_pass++; else { g_fail++; printf("FAIL: %s\n", msg); } } while (0)

int main(void)
{
    OUTPUT_CONTRACT c;
    char out[256];

    /* 1. title-like: single line + <= N characters (glyph comparator) */
    CHECK(OutputContractParse("Reply with a single line, \xE2\x89\xA4" "30 characters, nothing else.", &c), "parse title-like");
    CHECK(c.max_lines == 1 && c.max_chars == 30 && c.max_words == 0, "title-like bounds");
    CHECK(OutputContractCompose("run the full test suite and report which cases failed on linux", &c, out, sizeof(out)), "compose title-like");
    CHECK(OutputContractCheck(out, &c), "title-like verified");
    CHECK(strncmp(out, "Run the full test suite", 23) == 0, "title-like keeps the subject's own words");
    printf("title-like -> '%s'\n", out);

    /* 2. commit-subject-like: max 50 chars, one line */
    CHECK(OutputContractParse("Write the subject in one line of at most 50 chars.", &c), "parse commit-like");
    CHECK(c.max_lines == 1 && c.max_chars == 50, "commit-like bounds");

    /* 3. word bound, and a lower bound that must be ignored */
    CHECK(OutputContractParse("Use at least 3 words and no more than 5 words.", &c), "parse words");
    CHECK(c.max_words == 5, "lower bound ignored, upper kept");
    CHECK(OutputContractCompose("  \"please rename the helper function in util.c to compute_sum.\"  ", &c, out, sizeof(out)), "compose words");
    CHECK(strcmp(out, "Please rename the helper function") == 0, "quotes and period stripped, cut at 5 words");
    printf("words -> '%s'\n", out);

    /* 4. Spanish contract and Spanish subject (language follows the subject) */
    CHECK(OutputContractParse("Responde en una sola l\xC3\xADnea de 20 caracteres como m\xC3\xA1ximo.", &c), "parse spanish");
    CHECK(c.max_lines == 1 && c.max_chars == 20, "spanish bounds");
    CHECK(OutputContractCompose("ejecuta ollama list", &c, out, sizeof(out)), "compose spanish");
    CHECK(strcmp(out, "Ejecuta ollama list") == 0, "spanish subject kept");
    printf("spanish -> '%s'\n", out);

    /* 5. UTF-8 counting: accented characters count once */
    c.max_lines = 1; c.max_words = 0; c.max_chars = 12;
    CHECK(OutputContractCompose("revisi\xC3\xB3n del c\xC3\xB3" "digo fuente", &c, out, sizeof(out)), "compose utf8");
    CHECK(OutputContractCheck(out, &c), "utf8 verified");
    printf("utf8 -> '%s'\n", out);

    /* 5b. a cut never leaves a dangling short word */
    c.max_lines = 1; c.max_words = 0; c.max_chars = 50;
    CHECK(OutputContractCompose("revisa por que falla la compilacion de inventario.c", &c, out, sizeof(out)) &&
          strcmp(out, "Revisa por que falla la compilacion") == 0, "no dangling short word after cut");
    printf("cut -> '%s'\n", out);

    /* 6. no contract in ordinary text */
    CHECK(!OutputContractParse("You are a helpful assistant. Be concise and accurate.", &c), "no bound in plain text");
    CHECK(!OutputContractParse("There are 3 files and 2 functions.", &c), "counts of other things are not bounds");

    /* 8. other multi-byte glyphs (em dash, ellipsis) never stall the scan */
    CHECK(OutputContractParse("Titles \xE2\x80\x94 short \xE2\x80\xA6 a single line \xE2\x80\x94 \xE2\x89\xA4" "50 characters", &c) && c.max_lines == 1 && c.max_chars == 50, "glyphs between tokens");

    /* 7. multi-line subject: first non-empty line only */
    OutputContractParse("one line, 40 characters max", &c);
    CHECK(OutputContractCompose("\n\nfix the build\nthen push it", &c, out, sizeof(out)) && strcmp(out, "Fix the build") == 0, "first line only");

    /* 9. termination on arbitrary bytes (deterministic pseudo-random) */
    {
        unsigned x = 12345u;
        char buf[97];
        int ok = 1;
        for (int k = 0; k < 3000; k++)
        {
            for (int i = 0; i < 96; i++)
            {
                x = x * 1103515245u + 12345u;
                buf[i] = (char)(1 + (x >> 16) % 255);
            }
            buf[96] = '\0';
            OutputContractParse(buf, &c);
            c.max_chars = 1 + k % 40; c.max_words = k % 5; c.max_lines = 1;
            if (OutputContractCompose(buf, &c, out, sizeof(out)) && !OutputContractCheck(out, &c))
                ok = 0;
        }
        CHECK(ok, "random input: terminates, composed output always verifies");
    }

    printf("TEST RESULTS: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
