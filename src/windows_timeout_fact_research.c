#include "windows_timeout_fact_research.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
/* Must be used only after matching the reviewed sidecar SHA and Windows/CMD
   scope. -1 is deliberately not a finite duration. /nobreak concerns keyboard
   interruption, never exact elapsed wall-clock time. */
int WindowsTimeoutFactMatches(const char*command,long requested,const char*qualifier,int*nobreak){
 char num[32],extra[32]="",junk;int n=sscanf(command,"timeout /t %31s %31s %c",num,extra,&junk);
 int nb=n==2&&!strcmp(extra,"/nobreak");if(n!=1&&!nb)return 0;
 const char*p=num;if(*p=='-')p++;if(!*p||(*p=='0'&&p[1]))return 0;for(;*p;p++)if(*p<'0'||*p>'9')return 0;
 errno=0;char*end;long value=strtol(num,&end,10);if(errno||*end||value<0||value>99999||value!=requested)return 0;
 if(!strcmp(qualifier,"exact_wall")||(!strcmp(qualifier,"ignore_keyboard")&&!nb))return 0;
 *nobreak=nb;return 1;
}
