#include "attempt_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Read-only validator for offline evaluation. Never repairs a pair. */
int main(int argc, char **argv)
{
    if (argc != 4) { fputs("usage: attempt_capture_report root run count\n",stderr); return 2; }
    char *end=NULL;
    unsigned long n=strtoul(argv[3],&end,10);
    if (!argv[3][0] || !end || *end || n<1 || n>999 ||
        !AttemptCaptureValidate(argv[1],argv[2],(unsigned)n)) {
        fputs("capture unavailable\n",stderr); return 1;
    }
    printf("validated=%lu\n",n);
    return 0;
}
