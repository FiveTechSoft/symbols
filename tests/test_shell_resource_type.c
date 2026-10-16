#include <stdio.h>
#include <string.h>
#include "shell_resource_type.h"
int main(void)
{
    unsigned char valid[12]={'F',0,'i',0,'l',0,'e',0,0,0,0,0},bad[12];
    unsigned i;
    if(!resource_type_file(valid,8,1) || !resource_type_file(valid,10,5))return 1;
    if(resource_type_file(NULL,8,1) || resource_type_file(valid,8,0))return 1;
    for(i=0;i<=12;i++)if(i!=8 && i!=10 && resource_type_file(valid,i,1))return 1;
    for(i=0;i<10;i++) {
        memcpy(bad,valid,sizeof(bad));bad[i]^=1;
        if(resource_type_file(bad,10,1))return 1;
    }
    memcpy(bad,valid,sizeof(bad));bad[0]='f';if(resource_type_file(bad,8,1))return 1;
    memcpy(bad,valid,sizeof(bad));bad[2]=0;if(resource_type_file(bad,8,1))return 1;
    puts("RESOURCE_TYPE_COUNTED valid=2 negative_lengths_and_content=complete");return 0;
}
