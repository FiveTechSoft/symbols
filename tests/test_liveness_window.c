/* L1b: pure decision cells, not timing, scheduling or filesystem evidence. */
#include <stdio.h>
#include "liveness_window.h"
#define LIVENESS_NAME liveness_no_floor
#define LIVENESS_MUTANT_FLOOR
#include "liveness_window.h"
#undef LIVENESS_MUTANT_FLOOR
#define LIVENESS_NAME liveness_no_cap
#define LIVENESS_MUTANT_CAP
#include "liveness_window.h"
#undef LIVENESS_MUTANT_CAP

struct cell { const char *name; long ms; int enough, want; };
static const struct cell cells[] = {
    {"before_budget_low", 3999, 0, 1},
    {"before_budget_enough", 3999, 1, 1},
    {"budget_low", 4000, 0, 1},
    {"budget_enough", 4000, 1, 0},
    {"extension_low", 9000, 0, 1},
    {"extension_enough", 9000, 1, 0},
    {"before_cap_low", 29999, 0, 1},
    {"cap_low", 30000, 0, 0},
    {"past_cap_low", 30050, 0, 0},
    {"cap_enough", 30000, 1, 0}
};
static unsigned checks(int (*decision)(long,long,long,int))
{
    unsigned bad=0;
    for(unsigned i=0;i<sizeof(cells)/sizeof(cells[0]);i++)
        if(decision(cells[i].ms,4000,30000,cells[i].enough)!=cells[i].want)
            bad |= 1u<<i;
    return bad;
}
int main(void)
{
    unsigned base=checks(liveness_wait), floor=checks(liveness_no_floor), cap=checks(liveness_no_cap);
    if(base || floor!=((1u<<2)|(1u<<4)|(1u<<6)) || cap!=((1u<<7)|(1u<<8))) {
        printf("FAIL exact masks: base %x floor %x cap %x\n",base,floor,cap); return 1;
    }
    /* The Windows budget differs, but the same boundary predicates apply. */
    if(!liveness_wait(4999,5000,30000,1) || !liveness_wait(5000,5000,30000,0) ||
       liveness_wait(5000,5000,30000,1) || liveness_wait(30000,5000,30000,0)) return 1;
    printf("PASS liveness decision: 14 cells, floor mutant exact mask 54, cap mutant exact mask 180\n");
    return 0;
}
