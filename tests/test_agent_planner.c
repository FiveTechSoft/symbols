/* ============================================================
   test_agent_planner.c: Empirical evaluation of the Goal-Directed
                         STRIPS Task Planner & Dynamic Replanner.
   Pure ISO C11, zero tensors, zero backprop, fail-closed verification.
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "agent_planner.h"
#include <inttypes.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr, msg) do { \
    g_tests_run++; \
    if (expr) { \
        g_tests_passed++; \
        printf("  [PASS] %s\n", msg); \
    } else { \
        printf("  [FAIL] %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

/* ============================================================
   Test 1: Planner Initialization & Operator Registry
   ============================================================ */
static void test_planner_init(void)
{
    printf("\n=== Test 1: Planner Initialization & Operators ===\n");
    AGENT_PLANNER planner;
    int rc = AgentPlannerInit(&planner);
    TEST_ASSERT(rc == 1, "AgentPlannerInit succeeds");
    TEST_ASSERT(planner.operator_count == 8, "8 standard STRIPS operators registered");

    /* Verify essential operator properties */
    TEST_ASSERT(strcmp(planner.operators[0].name, "locate_symbol") == 0, "Op 0 is locate_symbol");
    TEST_ASSERT(strcmp(planner.operators[1].name, "inspect_code") == 0, "Op 1 is inspect_code");
    TEST_ASSERT(strcmp(planner.operators[3].name, "diagnose_error") == 0, "Op 3 is diagnose_error");
    TEST_ASSERT(strcmp(planner.operators[5].name, "apply_patch") == 0, "Op 5 is apply_patch");
    TEST_ASSERT(strcmp(planner.operators[6].name, "verify_build") == 0, "Op 6 is verify_build");
}

/* ============================================================
   Test 2: Optimal Forward Plan Formulation
   ============================================================ */
static void test_forward_planning(void)
{
    printf("\n=== Test 2: Optimal Forward Plan Formulation ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    /* Goal: Given known symbol, achieve verified regression tests and task completion */
    uint32_t init_state = PRED_SYMBOL_KNOWN;
    uint32_t goal_state = PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED;

    STRIPS_PLAN plan;
    int rc = AgentPlannerFormulate(&planner, "Refactor OldFunction and verify tests",
                                   init_state, goal_state, &plan);

    TEST_ASSERT(rc == 1, "AgentPlannerFormulate finds valid plan");
    TEST_ASSERT(plan.step_count == 7, "Optimal path found in exactly 7 steps");
    TEST_ASSERT(strcmp(plan.steps[0].op.name, "locate_symbol") == 0, "Step 1: locate_symbol");
    TEST_ASSERT(strcmp(plan.steps[1].op.name, "inspect_code") == 0, "Step 2: inspect_code");
    TEST_ASSERT(strcmp(plan.steps[2].op.name, "analyze_blast_radius") == 0, "Step 3: analyze_blast_radius");
    TEST_ASSERT(strcmp(plan.steps[3].op.name, "prepare_surgical_patch") == 0, "Step 4: prepare_surgical_patch");
    TEST_ASSERT(strcmp(plan.steps[4].op.name, "apply_patch") == 0, "Step 5: apply_patch");
    TEST_ASSERT(strcmp(plan.steps[5].op.name, "verify_build") == 0, "Step 6: verify_build");
    TEST_ASSERT(strcmp(plan.steps[6].op.name, "run_regression_tests") == 0, "Step 7: run_regression_tests");

    /* Format markdown schedule */
    char schedule[2048];
    int frc = AgentPlanFormatMarkdown(&plan, schedule, sizeof(schedule));
    TEST_ASSERT(frc == 1, "Formatted plan into markdown schedule");
    TEST_ASSERT(strstr(schedule, "Strategic STRIPS Plan") != NULL, "Schedule has title");
    TEST_ASSERT(strstr(schedule, "1. [ ] `locate_symbol`") != NULL, "Schedule lists steps");
    printf("Initial Plan Schedule:\n%s\n", schedule);
}

/* ============================================================
   Test 3: Plan Execution and Step Advancement
   ============================================================ */
static void test_plan_execution(void)
{
    printf("\n=== Test 3: Plan Execution & State Transitions ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    STRIPS_PLAN plan;
    AgentPlannerFormulate(&planner, "Step-by-step Execution Task",
                          PRED_SYMBOL_KNOWN, PRED_BUILD_VERIFIED, &plan);

    TEST_ASSERT(plan.step_count == 6, "Plan to build verification is 6 steps");
    TEST_ASSERT(!plan.is_achieved, "Plan starts unachieved");

    /* Step 1: locate_symbol */
    int rc1 = AgentPlanAdvance(&plan, true);
    TEST_ASSERT(rc1 == 1, "Step 1 executed successfully");
    TEST_ASSERT((plan.current_state & PRED_FILE_LOCATED) != 0, "File located in world state");

    /* Step 2: inspect_code */
    int rc2 = AgentPlanAdvance(&plan, true);
    TEST_ASSERT(rc2 == 1, "Step 2 executed successfully");
    TEST_ASSERT((plan.current_state & PRED_CODE_INSPECTED) != 0, "Code inspected in world state");

    /* Execute remaining steps to completion */
    while (plan.current_step_idx < plan.step_count)
    {
        AgentPlanAdvance(&plan, true);
    }

    TEST_ASSERT(plan.is_achieved, "Plan goal satisfied upon executing all steps");
    TEST_ASSERT((plan.current_state & PRED_BUILD_VERIFIED) != 0, "Build verified in final state");
}

/* ============================================================
   Test 4: Dynamic Replanning on Verification Failure
   ============================================================ */
static void test_dynamic_replanning(void)
{
    printf("\n=== Test 4: Dynamic Replanning on Verification Failure ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    STRIPS_PLAN plan;
    AgentPlannerFormulate(&planner, "Self-Healing Bug Fix Task",
                          PRED_SYMBOL_KNOWN,
                          PRED_TESTS_VERIFIED | PRED_TASK_COMPLETED,
                          &plan);

    /* Advance first 5 steps (up to apply_patch) */
    for (int i = 0; i < 5; i++)
    {
        AgentPlanAdvance(&plan, true);
    }

    /* Step 6: verify_build fails with exit code != 0! */
    TEST_ASSERT(strcmp(plan.steps[plan.current_step_idx].op.name, "verify_build") == 0,
                "Currently at verify_build step");
    int rc_fail = AgentPlanAdvance(&plan, false);
    TEST_ASSERT(rc_fail == 0, "verify_build records failure cleanly");

    /* Trigger dynamic replanning with compilation error diagnostic */
    int rc_replan = AgentPlannerReplanOnError(&planner, &plan,
                                              "gcc: error: undefined reference to 'NewFunc'");
    TEST_ASSERT(rc_replan == 1, "Dynamic replanning succeeds");

    /* Verify recovery schedule contains diagnosis and re-verification */
    char recovery_schedule[2048];
    AgentPlanFormatMarkdown(&plan, recovery_schedule, sizeof(recovery_schedule));
    printf("Replanned Recovery Schedule:\n%s\n", recovery_schedule);

    TEST_ASSERT(strstr(recovery_schedule, "`diagnose_error`") != NULL,
                "Recovery plan inserts diagnose_error step");
    TEST_ASSERT(strstr(recovery_schedule, "`verify_build`") != NULL,
                "Recovery plan re-inserts verify_build step");

    /* Execute through recovery steps to completion */
    while (plan.current_step_idx < plan.step_count)
    {
        AgentPlanAdvance(&plan, true);
    }
    TEST_ASSERT(plan.is_achieved, "Goal achieved through automated replanning loop");
}

/* ============================================================
   Test 5: Fail-Closed Boundaries & Trivial Goals
   ============================================================ */
static void test_fail_closed_boundaries(void)
{
    printf("\n=== Test 5: Fail-Closed Boundaries ===\n");
    AGENT_PLANNER planner;
    AgentPlannerInit(&planner);

    STRIPS_PLAN plan;

    /* Trivial goal: already in goal state */
    int rc_triv = AgentPlannerFormulate(&planner, "Trivial Task",
                                        PRED_TASK_COMPLETED, PRED_TASK_COMPLETED, &plan);
    TEST_ASSERT(rc_triv == 1, "Trivial goal formulation succeeds");
    TEST_ASSERT(plan.is_achieved, "Trivial goal marked achieved immediately");
    TEST_ASSERT(plan.step_count == 0, "Trivial goal requires 0 steps");

    /* Impossible goal with zero initial state */
    uint32_t impossible_goal = (1 << 30);
    int rc_imp = AgentPlannerFormulate(&planner, "Impossible Task", 0, impossible_goal, &plan);
    TEST_ASSERT(rc_imp == 0, "Unsatisfiable goal fails closed without infinite loop");

    /* Robustness checks */
    TEST_ASSERT(AgentPlannerInit(NULL) == 0, "NULL planner rejected");
    TEST_ASSERT(AgentPlannerFormulate(NULL, "", 0, 0, &plan) == 0, "NULL planner in formulate rejected");
    TEST_ASSERT(AgentPlanAdvance(NULL, true) == 0, "NULL plan in advance rejected");
}


/* Cost-optimality gold uses independent bounded enumeration/DP. */
/* Independent specification: enumerate all legal action sequences to length3.
   Fixtures use only bits0/1 and have optimal paths <=2; no overflow or caps. */
static uint32_t cost_repro_oracle(const AGENT_PLANNER*p,uint32_t state,uint32_t goal,uint32_t depth,uint32_t acc){
    uint32_t best=(state&goal)==goal?acc:UINT32_MAX;
    if(!depth)return best;
    for(uint32_t i=0;i<p->operator_count;i++){
        const STRIPS_OPERATOR*o=&p->operators[i];
        if((state&o->preconditions)==o->preconditions){
            uint32_t b=cost_repro_oracle(p,(state|o->add_effects)&~o->del_effects,goal,depth-1,acc+o->cost);
            if(b<best)best=b;
        }
    }
    return best;
}
static STRIPS_OPERATOR cost_repro_op(const char*name,uint32_t pre,uint32_t add,uint32_t del,uint32_t cost){
    STRIPS_OPERATOR o;memset(&o,0,sizeof o);snprintf(o.name,sizeof o.name,"%s",name);o.preconditions=pre;o.add_effects=add;o.del_effects=del;o.cost=cost;return o;
}
static int cost_repro_run(const char*name,AGENT_PLANNER*p,uint32_t initial,uint32_t goal,uint32_t expected){
    STRIPS_PLAN plan;uint32_t cost=0,state=initial;int valid=1;
    uint32_t gold=cost_repro_oracle(p,initial,goal,3,0);if(gold!=expected){fprintf(stderr,"ORACLE_MISMATCH\n");return 2;}
    int rc=AgentPlannerFormulate(p,name,initial,goal,&plan);
    if(rc){for(uint32_t i=0;i<plan.step_count;i++){
        STRIPS_OPERATOR*o=&plan.steps[i].op;valid&=(state&o->preconditions)==o->preconditions;
        state=(state|o->add_effects)&~o->del_effects;cost+=o->cost;
    }valid&=(state&goal)==goal;}else valid=0;
    int pass=valid&&cost==gold;
    printf("%s rc=%d valid=%d steps=%u cost=%u gold=%u %s ops=",name,rc,valid,plan.step_count,cost,gold,pass?"PASS":"FAIL");
    for(uint32_t i=0;i<plan.step_count;i++){printf("%s%s",i?",":"",plan.steps[i].op.name);}
    puts("");return pass?0:1;
}
static int cost_repro_main(void){
    AGENT_PLANNER p;int fails=0;memset(&p,0,sizeof p);p.operator_count=3;
    /* S0=00; G=bit1. direct costs3, viaM costs1+1. */
    p.operators[0]=cost_repro_op("direct",0,2,0,3);p.operators[1]=cost_repro_op("toM",0,1,0,1);p.operators[2]=cost_repro_op("fromM",1,2,0,1);
    fails+=cost_repro_run("shorter_not_cheaper",&p,0,2,2);
    /* Same-depth merge: first edge to same state costs3, later edge costs1. */
    p.operator_count=2;p.operators[0]=cost_repro_op("expensive",0,2,0,3);p.operators[1]=cost_repro_op("cheap",0,2,0,1);
    fails+=cost_repro_run("visited_blocks_cheaper",&p,0,2,1);
    /* Controls must stay green; changing queue order alone is not a fix. */
    STRIPS_OPERATOR tmp=p.operators[0];p.operators[0]=p.operators[1];p.operators[1]=tmp;
    fails+=cost_repro_run("cheap_first_control",&p,0,2,1);
    fails+=cost_repro_run("already_goal_control",&p,2,2,0);
    p.operator_count=3;p.operators[0]=cost_repro_op("direct",0,2,0,1);p.operators[1]=cost_repro_op("toM",0,1,0,1);p.operators[2]=cost_repro_op("fromM",1,2,0,1);
    fails+=cost_repro_run("unit_cost_control",&p,0,2,1);
    p.operator_count=1;p.operators[0]=cost_repro_op("highbit",0x80000000u,2,0x80000000u,1);
    fails+=cost_repro_run("uint32_highbit_control",&p,0x80000000u,2,1);
    printf("TOTAL %d failures of6\n",fails);return fails?1:0;
}

#define STATES 2048
static uint64_t cost_gold_dp[17][STATES];
static STRIPS_OPERATOR cost_gold_op(const char*n,uint32_t pre,uint32_t add,uint32_t del,uint32_t cost){STRIPS_OPERATOR o;memset(&o,0,sizeof o);snprintf(o.name,sizeof o.name,"%s",n);o.preconditions=pre;o.add_effects=add;o.del_effects=del;o.cost=cost;return o;}
static uint64_t cost_gold_oracle(const AGENT_PLANNER*p,uint32_t initial,uint32_t goal,uint32_t limit){
 memset(cost_gold_dp,0xff,sizeof cost_gold_dp);cost_gold_dp[0][initial]=0;uint64_t best=UINT64_MAX;
 for(uint32_t d=0;d<=limit;d++)for(uint32_t s=0;s<STATES;s++){
  if(cost_gold_dp[d][s]==UINT64_MAX)continue;
  if((s&goal)==goal&&cost_gold_dp[d][s]<best)best=cost_gold_dp[d][s];
  if(d==limit)continue;
  for(uint32_t i=0;i<p->operator_count;i++){const STRIPS_OPERATOR*o=&p->operators[i];if((s&o->preconditions)!=o->preconditions)continue;uint32_t t=(s|o->add_effects)&~o->del_effects;if(t>=STATES)return UINT64_MAX-1;uint64_t v=cost_gold_dp[d][s]+o->cost;if(v<cost_gold_dp[d+1][t])cost_gold_dp[d+1][t]=v;}
 }
 return best;
}
static int cost_gold_run(const char*n,AGENT_PLANNER*p,uint32_t initial,uint32_t goal,uint64_t gold,int expected_rc,int expected_steps,const char*first){
 uint64_t reference=cost_gold_oracle(p,initial,goal,16);if(reference!=gold){printf("ORACLE_BROKEN %s got=%"PRIu64" gold=%"PRIu64"\n",n,reference,gold);return 100;}
 STRIPS_PLAN plan;int rc=AgentPlannerFormulate(p,n,initial,goal,&plan);uint32_t state=initial;uint64_t cost=0;int valid=1;
 if(rc)for(uint32_t i=0;i<plan.step_count;i++){const STRIPS_OPERATOR*o=&plan.steps[i].op;if((state&o->preconditions)!=o->preconditions)valid=0;state=(state|o->add_effects)&~o->del_effects;cost+=o->cost;}
 if(rc&&(state&goal)!=goal)valid=0;
 int pass=rc==expected_rc&&(!rc||(valid&&cost==gold&&(expected_steps<0||plan.step_count==(uint32_t)expected_steps)&&(!first||strcmp(first,plan.steps[0].op.name)==0)));
 printf("%s rc=%d steps=%u replay_cost=%"PRIu64" cost_gold_oracle=%"PRIu64" valid=%d %s\n",n,rc,plan.step_count,cost,reference,valid,pass?"PASS":"FAIL");return !pass;
}
/* dual-rail low5=ON, high5=OFF; increment operators iff current bitOFF and all lowerbitsON. */
static uint32_t cost_gold_encode(uint32_t n){return n|((31^n)<<5);}
static void cost_gold_counter(AGENT_PLANNER*p){memset(p,0,sizeof *p);p->operator_count=5;for(uint32_t i=0;i<5;i++){uint32_t bit=1u<<i,lower=bit-1;char name[16];snprintf(name,sizeof name,"inc%u",i);p->operators[i]=cost_gold_op(name,(bit<<5)|lower,bit|(lower<<5),(bit<<5)|lower,1);}}
static int cost_gold_main(void){AGENT_PLANNER p;int failures=0;
 memset(&p,0,sizeof p);p.operator_count=3;p.operators[0]=cost_gold_op("noop",0,0,0,0);p.operators[1]=cost_gold_op("zero",0,2,0,0);p.operators[2]=cost_gold_op("positive",0,2,0,1);failures+=cost_gold_run("zero_noop",&p,0,2,0,1,1,"zero");
 p.operators[0]=cost_gold_op("direct",0,2,0,UINT32_MAX);p.operators[1]=cost_gold_op("toM",0,1,0,UINT32_MAX-1);p.operators[2]=cost_gold_op("fromM",1,2,0,10);failures+=cost_gold_run("uint32sum",&p,0,2,UINT32_MAX,1,1,"direct");
 p.operators[0].cost=2;p.operators[1].cost=1;p.operators[2].cost=1;failures+=cost_gold_run("equal_cost_fewer_steps",&p,0,2,2,1,1,"direct");
 p.operator_count=2;p.operators[0]=cost_gold_op("first",0,2,0,1);p.operators[1]=cost_gold_op("second",0,2,0,1);failures+=cost_gold_run("equal_cost_lex",&p,0,2,1,1,1,"first");
 cost_gold_counter(&p);p.operators[p.operator_count++]=cost_gold_op("shortcut",cost_gold_encode(0),cost_gold_encode(16),~cost_gold_encode(16)&1023,100);failures+=cost_gold_run("depth_tradeoff",&p,cost_gold_encode(0),31,115,1,16,"shortcut");
 cost_gold_counter(&p);failures+=cost_gold_run("limit16",&p,cost_gold_encode(0),16,16,1,16,NULL);failures+=cost_gold_run("limit17",&p,cost_gold_encode(0),17,UINT64_MAX,0,-1,NULL);
 memset(&p,0,sizeof p);p.operator_count=11;p.operators[0]=cost_gold_op("expensive_goal",0,1024,0,1000);for(uint32_t i=0;i<10;i++){char n[16];snprintf(n,sizeof n,"independent%u",i);p.operators[i+1]=cost_gold_op(n,0,1u<<i,0,1);}
 /* Resource gold is rejection, despite mathematical routecost1000. This is a proposed new explicit contract, not an old-C bug claim. */
 failures+=cost_gold_run("resource_exhaustion_must_reject",&p,0,1024,1000,0,-1,NULL);
 printf("TOTAL %d failures of8\n",failures);return failures?1:0;
}

typedef struct{int valid;uint64_t cost;uint32_t seq[16];} cost_fuzz_REF;
static cost_fuzz_REF cost_fuzz_tab[17][8];static uint32_t cost_fuzz_seed=0x42695;static uint32_t cost_fuzz_rnd(void){cost_fuzz_seed=cost_fuzz_seed*1664525u+1013904223u;return cost_fuzz_seed;}
static int cost_fuzz_lex(const uint32_t*a,const uint32_t*b,uint32_t n){for(uint32_t i=0;i<n;i++){if(a[i]<b[i])return-1;if(a[i]>b[i])return 1;}return 0;}
static int cost_fuzz_main(void){uint32_t validcases=0;
for(uint32_t z=0;z<5000;z++){
 AGENT_PLANNER p;memset(&p,0,sizeof p);p.operator_count=1+cost_fuzz_rnd()%6;uint32_t init=cost_fuzz_rnd()%8,goal=cost_fuzz_rnd()%8;
 for(uint32_t i=0;i<p.operator_count;i++){STRIPS_OPERATOR*o=&p.operators[i];o->preconditions=(cost_fuzz_rnd()>>12)%8;o->add_effects=(cost_fuzz_rnd()>>12)%8;o->del_effects=(cost_fuzz_rnd()>>12)%8;o->cost=(cost_fuzz_rnd()>>12)%4;snprintf(o->name,sizeof o->name,"op%u",i);}
 memset(cost_fuzz_tab,0,sizeof cost_fuzz_tab);cost_fuzz_tab[0][init].valid=1;cost_fuzz_REF best;memset(&best,0,sizeof best);uint32_t bestdepth=0;
 for(uint32_t d=0;d<=16;d++)for(uint32_t s=0;s<8;s++){
  cost_fuzz_REF*cur=&cost_fuzz_tab[d][s];if(!cur->valid)continue;
  if((s&goal)==goal&&(!best.valid||cur->cost<best.cost||(cur->cost==best.cost&&(d<bestdepth||(d==bestdepth&&cost_fuzz_lex(cur->seq,best.seq,d)<0))))){best=*cur;bestdepth=d;}
  if(d==16)continue;
  for(uint32_t i=0;i<p.operator_count;i++){const STRIPS_OPERATOR*o=&p.operators[i];if((s&o->preconditions)!=o->preconditions)continue;uint32_t t=(s|o->add_effects)&~o->del_effects;cost_fuzz_REF cand=*cur;cand.cost+=o->cost;cand.seq[d]=i;cost_fuzz_REF*dst=&cost_fuzz_tab[d+1][t];if(!dst->valid||cand.cost<dst->cost||(cand.cost==dst->cost&&cost_fuzz_lex(cand.seq,dst->seq,d+1)<0))*dst=cand;}
 }
 STRIPS_PLAN plan;int rc=AgentPlannerFormulate(&p,"fuzz",init,goal,&plan);uint64_t cost=0;int valid=1;uint32_t s=init;
 if(rc)for(uint32_t i=0;i<plan.step_count;i++){STRIPS_OPERATOR*o=&plan.steps[i].op;cost+=o->cost;if((s&o->preconditions)!=o->preconditions)valid=0;s=(s|o->add_effects)&~o->del_effects;if(strcmp(o->name,p.operators[best.seq[i]].name))valid=0;}
 if(rc!=best.valid||(rc&&(!valid||(s&goal)!=goal||cost!=best.cost||plan.step_count!=bestdepth))){printf("FAIL z%u seed%u init%u goal%u rc%d ref%d cost%"PRIu64" gold%"PRIu64" steps%u goldsteps%u\n",z,cost_fuzz_seed,init,goal,rc,best.valid,cost,best.cost,plan.step_count,bestdepth);return 1;}
 validcases++;
}
printf("PASS independent exact-depthDP/replay/cost/steps/lex: %u deterministic registries, <=6operators,3bits,depth16,cost0..3\n",validcases);return 0;}

int main(void)
{
    printf("========================================================\n");
    printf("  GOAL-DIRECTED STRIPS TASK PLANNER TEST SUITE\n");
    printf("========================================================\n");

    test_planner_init();
    test_forward_planning();
    test_plan_execution();
    test_dynamic_replanning();
    test_fail_closed_boundaries();
    TEST_ASSERT(cost_repro_main() == 0, "Cost counterexamples and controls");
    TEST_ASSERT(cost_gold_main() == 0, "Cost/depth/overflow/resource gold");
    TEST_ASSERT(cost_fuzz_main() == 0, "5000 exact-depth DP differential cases");

    printf("\n--------------------------------------------------------\n");
    printf("Results: %d/%d tests passed\n", g_tests_passed, g_tests_run);
    printf("========================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
