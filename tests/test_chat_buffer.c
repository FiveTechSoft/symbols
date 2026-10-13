/* Caller capacity controls presentation only, never answer acceptance. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chat.h"
static int failures;
static void check(int yes,const char*name){printf("%s %s\n",yes?"PASS":"FAIL",name);if(!yes)failures++;}
static CHAT *create(const char *path){CHAT*c=calloc(1,sizeof(*c));if(!c)return NULL;ChatInit(c,path);c->episodic.auto_save=0;return c;}
static void destroy(CHAT*c){if(c){ChatDestroy(c);free(c);}}
static void compare(const char*path,const char*query,int mode,size_t capacity)
{
 CHAT*a=create(path),*b=create(path);char wide[16384],small[16384];GoalCause ca=CAUSE_NONE,cb=CAUSE_NONE;int ra,rb;
 check(a&&b&&a->ntfiles==1&&b->ntfiles==1,"buffer fixture loaded");if(!a||!b){destroy(a);destroy(b);return;}
 memset(wide,0,sizeof(wide));memset(small,0xa5,sizeof(small));
 if(mode==0){ra=ChatResolveLine(a,query,wide,sizeof(wide),NULL,0,NULL,0,&ca);rb=ChatResolveLine(b,query,small,capacity,NULL,0,NULL,0,&cb);}
 else if(mode==1){ra=ChatTryTextLine(a,query,wide,sizeof(wide));small[0]=0;rb=ChatTryTextLine(b,query,small,capacity);}
 else{ra=ChatHandleToBuf(a,query,wide,sizeof(wide));rb=ChatHandleToBuf(b,query,small,capacity);}
 check(ra==rb&&ca==cb,"buffer does not change status/cause");
 if(mode==0||ra){size_t n=strlen(wide);if(n>=capacity)n=capacity-1;check(strlen(small)==n&&memcmp(small,wide,n)==0,"small output is decided wide prefix");check((unsigned char)small[capacity]==0xa5,"buffer writes stay within capacity");}
 check(a->tnw==b->tnw&&a->ntshown==b->ntshown&&memcmp(a->tshown,b->tshown,a->ntshown*sizeof(*a->tshown))==0,"buffer does not change selection/cache");
 destroy(a);destroy(b);
}
int main(void)
{
 const char*path="chat_buffer_fixture.txt";FILE*f=fopen(path,"wb");if(!f)return 2;
 fputs("And those gamma rays came just 1.7 seconds after the gravitational wave signal.\n"
       "Primer postulado (principio de relatividad) explains distant observations.\n"
       "Second postulado includes a distinct statement.\n",f);fclose(f);
 const char*queries[]={"seconds","what is seconds?","postulado","que es postulado?","zzyyzzbufferabsent20261013","hola"};size_t caps[]={1,8,14,15,16,32,64,4096,8192};
 for(unsigned m=0;m<3;m++)for(unsigned q=0;q<sizeof(queries)/sizeof(*queries);q++)for(unsigned c=0;c<sizeof(caps)/sizeof(*caps);c++)compare(path,queries[q],m,caps[c]);
 /* This fixture must exercise quotes, not pass by abstaining in both runs. */
 {CHAT*c=create(path);char out[4096];GoalCause cause;int r=ChatResolveLine(c,"seconds",out,sizeof(out),NULL,0,NULL,0,&cause);check(r==GOAL_ANSWER&&strstr(out,"1 . 7 seconds"),"wide quote acceptance gold retained");destroy(c);}
 remove(path);printf("test_chat_buffer: %d failed\n",failures);return failures?1:0;
}
