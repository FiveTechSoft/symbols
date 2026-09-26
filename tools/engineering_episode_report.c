#include "engineering_episode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The CLI delegates to this bounded, read-only function. Tests call it
   directly, without a platform shell or subprocess launch. */
int EpisodeReport(const char *path, FILE *out, FILE *error)
{
    if(!path || !out || !error)return 2;
    EPISODE_STORE *s=(EPISODE_STORE *)calloc(1,sizeof(*s));if(!s)return 2;
    int rc=EpisodeLoad(path,s);
    if(rc!=1) {fprintf(error,"episode store %s: %s (coverage unknown)\n",path,rc==0?"absent":"invalid or unreadable");free(s);return 1;}
    unsigned verified=0,refuted=0,abstained=0,other=0;
    unsigned long long builds=0,probes=0,calls=0;
    for(size_t i=0;i<s->count;++i) {
        ENGINEERING_EPISODE *r=&s->rows[i];
        if(!strcmp(r->outcome,"verified"))++verified;
        else if(!strcmp(r->outcome,"refuted"))++refuted;
        else if(!strcmp(r->outcome,"abstained"))++abstained;
        else ++other;
        builds+=r->candidate_builds;probes+=r->probes;calls+=r->tool_calls;
    }
    fprintf(out,"schema=1 records=%zu verified=%u refuted=%u abstained=%u other=%u candidate_builds=%llu probes=%llu tool_calls=%llu\n",
           s->count,verified,refuted,abstained,other,builds,probes,calls);
    fprintf(out,"verified means mechanical-oracle pass only; no claim of semantic correctness or current workspace state\n");
    free(s);return 0;
}

#ifndef EPISODE_REPORT_NO_MAIN
int main(int argc, char **argv)
{
    if(argc!=2) {fprintf(stderr,"usage: engineering_episode_report <store>\n");return 2;}
    return EpisodeReport(argv[1],stdout,stderr);
}
#endif
