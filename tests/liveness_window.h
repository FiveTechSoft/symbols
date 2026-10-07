/* Test-only stop decision. Counts remain guarded again after joining writers.
   Include repeatedly with LIVENESS_NAME to build exact decision mutants. */
#ifndef LIVENESS_NAME
#define LIVENESS_NAME liveness_wait
#endif
static int LIVENESS_NAME(long elapsed, long budget, long cap, int enough)
{
#ifdef LIVENESS_MUTANT_FLOOR
    (void)cap; (void)enough;
    return elapsed < budget;
#elif defined(LIVENESS_MUTANT_CAP)
    (void)cap;
    return elapsed < budget || !enough;
#else
    return elapsed < budget || (!enough && elapsed < cap);
#endif
}
#undef LIVENESS_NAME
