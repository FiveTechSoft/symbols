#include "c_contract.h"
#include <stdio.h>
#include <limits.h>
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
    /* Closed value grammar: one initializer, one format conversion, exact bytes. */
    const char *value="#include <stdio.h>\nint main(void){ int count = 7; printf(\"seen=%d\\n\", count); return 0; }\n";
    n=run(value,"seen=-42\n",c);
    CHECK(n==1 && c[0].tier==4 && !strcmp(c[0].rule,"answer_value"));
    CHECK(n==1 && c[0].text && strstr(c[0].text,"int count = -42;")!=NULL);
    CHECK(n==1 && c[0].text && strstr(c[0].text,"printf(\"seen=%d\\n\", count)")!=NULL);
    CContractFree(c,n);
    n=run(value,"seen=7\n",c);CHECK(n==0);CContractFree(c,n);
    const char *impossible[]={"seen=+8\n","seen=08\n","seen=-0\n","seen=8", "seen=8\nextra", "other=8\n", "seen=2147483648\n"};
    for(size_t i=0;i<sizeof(impossible)/sizeof(impossible[0]);i++) {
        n=run(value,impossible[i],c);CHECK(n==0);CContractFree(c,n);
    }
    n=run("#include <stdio.h>\nint main(){int y=0;printf(\"%%i=%i!\",y);return 0;}\n","%i=-8!",c);
    CHECK(n==1 && !strcmp(c[0].rule,"answer_value") && strstr(c[0].text,"int y=-8;"));CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){int q=1;printf(\"%u\",q);return 0;}\n","2147483647",c);
    CHECK(n==1 && !strcmp(c[0].rule,"answer_value") && strstr(c[0].text,"int q=2147483647;"));CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){int q=1;printf(\"%u\",q);return 0;}\n","2147483648",c);
    CHECK(n==0);CContractFree(c,n);
    n=run("#include <stdio.h>\nint main(void){int z=-1;printf(\"%u\",z);return 0;}\n","12",c);
    for(int i=0;i<n;i++)CHECK(strcmp(c[i].rule,"answer_value"));CContractFree(c,n);
    const char *outside[]={
        "#include <stdio.h>\nint main(void){int z=1;z++;printf(\"%d\",z);return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=1;printf(\"%d/%d\",z,z);return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=1;printf(\"%s\",z);return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=1;printf(\"%04d\",z);return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=1;printf(\"%.2d\",z);return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=2+3;printf(\"%d\",z);return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=1;printf(\"%d\",z);return 0;}\nint helper(void){return 0;}\n",
        "#include <stdio.h>\nint main(void){int z=1;printf(\"%d\",z);return 0;}\n/*more*/\n",
    };
    for(size_t i=0;i<sizeof(outside)/sizeof(outside[0]);i++) {
        n=run(outside[i],"12",c);
        for(int j=0;j<n;j++)CHECK(strcmp(c[j].rule,"answer_value"));
        CContractFree(c,n);
    }
    if(failures)return 1;puts("OK");return 0;
}
