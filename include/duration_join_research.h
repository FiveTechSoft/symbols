#ifndef DURATION_JOIN_RESEARCH_H
#define DURATION_JOIN_RESEARCH_H
#include <stddef.h>
/* Opt-in research only. Explicit envelope or bounded English duration question, not arbitrary QA.
   0 means not this diagnostic syntax. 1 means handled with evidence or a
   stated unresolved condition. No shell/tool execution occurs. */
int DurationJoinResearch(const char *query,const char *corpus,const char *fact,
                         char *out,size_t capacity);
#endif
