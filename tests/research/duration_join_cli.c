#include "duration_join_research.h"
#include <stdio.h>
int main(int argc,char**argv){if(argc!=4)return 2;char out[8192];int r=DurationJoinResearch(argv[3],argv[1],argv[2],out,sizeof(out));printf("HANDLED%d\n%s\n",r,r?out:"");return 0;}
