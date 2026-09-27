#include "c_contract.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c);failures++; } } while(0)
static int run(const char *src,const char *goal,C_CAND *out)
{
    C_CONTRACT c={0};c.has_out=1;
    snprintf(c.out,sizeof(c.out),"%s",goal);
    return CContractStaticCandidates(src,&c,out,96);
}
int main(void)
{
    C_CAND c[96]={0};int n;
    const char *base="#include <stdio.h>\nint main(void){ printf(\"ship\\n\"); return 0; }\n";
    n=run(base,"arrive\n",c);
    CHECK(n==1 && c[0].tier==4 && !strcmp(c[0].rule,"answer_literal"));
    CHECK(n==1 && strstr(c[0].text,"printf(\"arrive\\n\")")!=NULL);
    CContractFree(c,n);
    n=run(base,"ship\n",c);CHECK(n==0);CContractFree(c,n);
    n=run(base,"A\nB\r\t\"\\%",c);
    CHECK(n==1 && strstr(c[0].text,"printf(\"A\\nB\\r\\t\\\"\\\\%%\")"));
    CContractFree(c,n);
    n=run(base,"A\001B",c);CHECK(n==0);CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ puts(\"ship\"); return 0; }\n","arrive\n",c);
    CHECK(n==1 && strstr(c[0].text,"puts(\"arrive\")"));CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ puts(\"ship\"); return 0; }\n","arrive",c);
    CHECK(n==0);CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ printf(\"ship\"); printf(\"again\"); return 0; }\n","arrive",c);
    for(int i=0;i<n;i++) CHECK(strcmp(c[i].rule,"answer_literal"));
    CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ printf(\"%d\",1); return 0; }\n","arrive",c);
    for(int i=0;i<n;i++) CHECK(strcmp(c[i].rule,"answer_literal"));CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ if(1) printf(\"ship\"); return 0; }\n","arrive",c);
    for(int i=0;i<n;i++) CHECK(strcmp(c[i].rule,"answer_literal"));CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ printf(\"ship\"); return 0; }\nint helper(void){ return 0; }\n","arrive",c);
    for(int i=0;i<n;i++) CHECK(strcmp(c[i].rule,"answer_literal"));CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){ printf(\"ship\"); return 0; }\n","",c);
    CHECK(n==0);CContractFree(c,n);
    n=run("int helper(void){return 0;}\nint main(void){return helper();}\n","arrive",c);
    for(int i=0;i<n;i++) CHECK(strcmp(c[i].rule,"answer_literal"));
    CContractFree(c,n);
    if(failures)return 1;puts("OK");return 0;
}
