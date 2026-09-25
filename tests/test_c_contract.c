/* test_c_contract.c: the wanted stdout is read from the task (current-state
   clauses skipped) and single-edit candidates are generated for C. */
#include "c_contract.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int has(C_CAND *c, int n, const char *rule, int tier, const char *needle)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(c[i].rule, rule) && c[i].tier == tier && (!needle || strstr(c[i].text, needle)))
            return 1;
    return 0;
}

int main(void)
{
    C_CONTRACT k;
    static C_CAND c[96];
    int n;

    /* "which is N" after a print key; the ", but it prints" tail is the current state */
    CHECK(CContractParse("The program should print the sum of 1..10, which is 55, but it prints 45. Fix it.", &k));
    CHECK(!strcmp(k.out, "55"));
    n = CContractCandidates("int sum_to(int n) { int s = 0; for (int i = 1; i < n; i++) s += i; return s; }\n"
                            "int main(void) { return sum_to(3) != 3; }\n", &k, 0, c, 96);
    CHECK(has(c, n, "boundary", 1, "i <= n"));
    CHECK(!has(c, n, "int_literal", 4, "sum_to(4)"));   /* main is the oracle when other functions exist */
    CContractFree(c, n);

    CHECK(CContractParse("Fix factorial so that the output is `120`.", &k) && !strcmp(k.out, "120"));
    n = CContractCandidates("long fact(int n) { long r = 0; for (int i = 2; i <= n; i++) r *= i; return r; }\n", &k, 0, c, 96);
    CHECK(has(c, n, "init_mul", 2, "long r = 1;"));
    CContractFree(c, n);

    /* A declaration with two initialized declarators has two distinct
       init_mul sites, even when the first initializer ends at a comma. */
    CHECK(CContractParse("It must print 3.", &k));
    n = CContractCandidates("#include <stdio.h>\nint main(void){ int p=0,q=0; p*=3; q*=3; printf(\"%d\\n\", p+q); return 0; }\n", &k, 1, c, 96);
    CHECK(has(c, n, "init_mul", 2, "int p=1,q=0;") && has(c, n, "init_mul", 2, "int p=0,q=1;"));
    CContractFree(c, n);
    n = CContractCandidates("int main(void){ int a=0 , b=0 ; a *= 4; b *= 4; return a+b; }\n", &k, 1, c, 96);
    CHECK(has(c, n, "init_mul", 2, "a=1 , b=0") && has(c, n, "init_mul", 2, "a=0 , b=1"));
    CContractFree(c, n);

    CHECK(CContractParse("After swap the program must print \"2 1\".", &k) && !strcmp(k.out, "2 1"));
    n = CContractCandidates("void swap(int *a, int *b) { int t = *a; *a = *b; *b = *a; }\n", &k, 0, c, 96);
    CHECK(has(c, n, "stale_swap", 2, "*b = t;"));
    CContractFree(c, n);

    CHECK(CContractParse("average() is wrong: the program must print 2.50 for the values 1 2 3 4.", &k) && !strcmp(k.out, "2.50"));
    n = CContractCandidates("double average(const int *v, int n) { int s = 0; for (int i = 0; i < n; i++) s += v[i]; return s / n; }\n",
                            &k, 0, c, 96);
    CHECK(has(c, n, "int_div", 2, "return (double)s / n;"));
    CHECK(has(c, n, "index_shift", 2, "v[i - 1]") && has(c, n, "index_shift", 4, "v[i + 1]"));
    CContractFree(c, n);

    CHECK(CContractParse("reverse() is off by one: the program should print \"olleh\".", &k));
    n = CContractCandidates("void r(char *s, int n) { for (int i = 0; i < n / 2; i++) { char t = s[i]; s[i] = s[n - i]; s[n - i] = t; } }\n",
                            &k, 0, c, 96);
    CHECK(has(c, n, "index_shift", 2, "s[i] = s[n - i - 1]; s[n - i - 1] = t;"));   /* every occurrence in the function */
    CContractFree(c, n);

    /* a candidate may not write the wanted output into the code */
    CHECK(CContractParse("It must print 7.", &k));
    n = CContractCandidates("int f(void) { return 6; }\n", &k, 0, c, 96);
    CHECK(!has(c, n, "int_literal", 4, "return 7;") && has(c, n, "int_literal", 4, "return 5;"));
    CContractFree(c, n);

    /* comments, strings and preprocessor lines are not code */
    CHECK(CContractParse("It should print ok.", &k) && !strcmp(k.out, "ok"));
    n = CContractCandidates("#include <stdio.h>\nint f(int a) { /* a < b */ puts(\"x<y\"); return a == 1; }\n", &k, 0, c, 96);
    CHECK(has(c, n, "equality", 3, "a != 1") && !has(c, n, "boundary", 1, NULL));
    CContractFree(c, n);

    /* no wanted clause, current state only, a non-zero exit, or a file: nothing */
    CHECK(!CContractParse("It prints 5 right now. Add a README.", &k));
    CHECK(!CContractParse("Add a comment describing main. The program prints 7.", &k));
    CHECK(!CContractParse("On bad input it must exit with status 2 and print usage.", &k));
    CHECK(!CContractParse("The program should write 5 to the file out.txt and print nothing else.", &k));
    CHECK(!CContractParse("It should print `a` and must print `b`.", &k));   /* two values: no contract */

    /* varied goal wording: other output verbs, labels, goal markers, and a
       goal clause that names exactly one value */
    CHECK(CContractParse("It should display 15.", &k) && !strcmp(k.out, "15"));
    CHECK(CContractParse("Correct output: 9", &k) && !strcmp(k.out, "9"));
    CHECK(CContractParse("The output should be: 15", &k) && !strcmp(k.out, "15"));
    CHECK(CContractParse("Fix it so it prints 15.", &k) && !strcmp(k.out, "15"));
    CHECK(CContractParse("The average printed must equal 2.5.", &k) && !strcmp(k.out, "2.5"));
    CHECK(CContractParse("The program should report a maximum of 9.", &k) && !strcmp(k.out, "9"));
    CHECK(CContractParse("The count displayed should be 7, not 6.", &k) && !strcmp(k.out, "7"));
    CHECK(CContractParse("Running it gives 14; it should give 15.", &k) && !strcmp(k.out, "15"));
    CHECK(!CContractParse("The function should take 3 arguments.", &k));        /* nothing about output */
    CHECK(!CContractParse("The buffer should be 64 bytes.", &k));
    CHECK(!CContractParse("Write the result to out.txt; it should contain 42.", &k));   /* a file */
    CHECK(!CContractParse("It shows 6 right now.", &k));

    /* closed-form reason when nothing was read */
    CHECK(!CContractParse("Refactor the parser for readability.", &k) && !strcmp(k.why, "nogoal"));
    CHECK(!CContractParse("It currently should print 5 but prints 4.", &k) && !strcmp(k.why, "now"));
    CHECK(!CContractParse("The program must exit with status 2 on bad input.", &k) && !strcmp(k.why, "exit"));
    CHECK(!CContractParse("The output file must contain the total.", &k) && !strcmp(k.why, "file"));
    CHECK(!CContractParse("The program should print the sum of the list.", &k) && !strcmp(k.why, "noval"));
    CHECK(!CContractParse("It should print `5`. It must print `6`.", &k) && !strcmp(k.why, "multi"));
    CHECK(CContractParse("It should print `5`.", &k) && k.why[0] == '\0');
    /* explicit no-output disclaimers veto nearby quoted samples */
    CHECK(!CContractParse("Fix a C memory bug; no expected output is specified; stdout 5 was observed.", &k) && !strcmp(k.why, "no_output"));
    CHECK(!CContractParse("A test once printed 8; there is no specified output to assert.", &k));
    CHECK(!CContractParse("This task has no concrete stdout goal; 7 is only a sample run.", &k));
    CHECK(!CContractParse("Fix the leak without any expected stdout; it once printed 9.", &k));
    CHECK(!CContractParse("Expected stdout is unknown; observed output was 10.", &k));
    CHECK(CContractParse("The program should print 7, not 6.", &k) && !strcmp(k.out,"7"));
    printf("%s\n", fails ? "FAILED" : "OK");
    return fails != 0;
}
