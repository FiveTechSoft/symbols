/* Static C candidate export. This tool never compiles or executes target C. */
#include "c_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv)
{
    if(argc!=4)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
    long n=ftell(f);
    if(n<0||n>65536||fseek(f,0,SEEK_SET)){fclose(f);return 2;}
    char *src=malloc((size_t)n+1);if(!src){fclose(f);return 2;}
    int ok=fread(src,1,(size_t)n,f)==(size_t)n;
    if(fclose(f))ok=0;
    src[n]=0;
    if(!ok||memchr(src,0,(size_t)n)){free(src);return 2;}
    C_CONTRACT c={0};
    size_t goal_len=strlen(argv[3]);
    if(goal_len>=sizeof(c.out)){free(src);return 2;}
    if(goal_len){
        memcpy(c.out,argv[3],goal_len+1);c.has_out=1;
    }
    C_CAND cand[96]={0};
    int count=CContractStaticCandidates(src,&c,cand,96);
    for(int i=0;i<count;i++){
        char out[1024];
        int z=snprintf(out,sizeof(out),"%s/%03d.c",argv[2],i);
        if(z<=0||z>=(int)sizeof(out)){ok=0;break;}
        FILE *w=fopen(out,"wb");if(!w){ok=0;break;}
        size_t len=strlen(cand[i].text);
        if(fwrite(cand[i].text,1,len,w)!=len)ok=0;
        if(fclose(w))ok=0;
        if(!ok)break;
        printf("%d\t%s\t%03d.c\n",cand[i].tier,cand[i].rule,i);
    }
    CContractFree(cand,count);free(src);
    return ok?0:2;
}
