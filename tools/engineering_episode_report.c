#include "engineering_episode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv)
{
    if(argc!=2) {fprintf(stderr,"usage: engineering_episode_report <store>\n");return 2;}
    EPISODE_STORE *s=(EPISODE_STORE *)calloc(1,sizeof(*s));if(!s)return 2;
    int rc=EpisodeLoad(argv[1],s);
    if(rc!=1) {fprintf(stderr,"episode store %s: %s (coverage unknown)\n",argv[1],rc==0?"absent":"invalid or unreadable");free(s);return 1;}
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
    printf("schema=1 records=%zu verified=%u refuted=%u abstained=%u other=%u candidate_builds=%llu probes=%llu tool_calls=%llu\n",
           s->count,verified,refuted,abstained,other,builds,probes,calls);
    puts("verified means mechanical-oracle pass only; no claim of semantic correctness or current workspace state");
    free(s);return 0;
}
