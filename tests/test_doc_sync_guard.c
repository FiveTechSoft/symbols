#include "task_ops.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void put(const char*d,const char*n,const char*t){char p[1024];FILE*f;snprintf(p,sizeof(p),"%s/%s",d,n);f=fopen(p,"wb");if(!f)exit(2);fwrite(t,1,strlen(t),f);fclose(f);}
static int same(const char*d,const char*n,const char*t){char p[1024],b[4096];FILE*f;size_t z;snprintf(p,sizeof(p),"%s/%s",d,n);f=fopen(p,"rb");if(!f)return 0;z=fread(b,1,sizeof(b),f);fclose(f);return z==strlen(t)&&!memcmp(b,t,z);}
int main(void){TASK_OPS_REPORT r;int kept,wrong;unsigned mask=0;
#ifdef _WIN32
 _putenv("SYMBOLS_TASK_OPS_MEMORY=0");_putenv("SYMBOLS_REFLEXION=0");
 if(system("if exist doc_sync_gate_scratch rmdir /s /q doc_sync_gate_scratch")!=0)return 2;
#else
 setenv("SYMBOLS_TASK_OPS_MEMORY","0",1);setenv("SYMBOLS_REFLEXION","0",1);
 if(system("rm -rf doc_sync_gate_scratch")!=0)return 2;
#endif
 _mkdir("doc_sync_gate_scratch");
#ifdef DOC_SYNC_MUTANT
 TaskOpsTestDocSyncGuard(0);
#endif

{const char*d="doc_sync_gate_scratch/misleading_identifier"; _mkdir(d);
put(d,"app.c","int max_retries = 3;\nint main(void) { return 0; }\n");
put(d,"notes.md","The Symbols setting is max_replans.\n");
kept=TaskOpsSolve(d,"notes.md mentions max_replans but app.c uses max_retries. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"app.c","int max_retries = 3;\nint main(void) { return 0; }\n");
wrong |= !same(d,"notes.md","The Symbols setting is max_replans.\n");
wrong |= strstr(r.reason,"doc_sync requires verified documentation relation")==NULL;
if(wrong){failures++;mask|=1u<<0;}printf("%s misleading_identifier kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/real_flag_lost"; _mkdir(d);
put(d,"app.c","#include <string.h>\nint main(int c, char **v) { return c > 1 && !strcmp(v[1], \"--quiet\") ? 0 : 0; }\n");
put(d,"USAGE.md","Pass --silent to mute it. --silent is optional.\n");
kept=TaskOpsSolve(d,"USAGE.md says --silent but app.c checks --quiet. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"app.c","#include <string.h>\nint main(int c, char **v) { return c > 1 && !strcmp(v[1], \"--quiet\") ? 0 : 0; }\n");
wrong |= !same(d,"USAGE.md","Pass --silent to mute it. --silent is optional.\n");
wrong |= strstr(r.reason,"doc_sync requires verified documentation relation")==NULL;
if(wrong){failures++;mask|=1u<<1;}printf("%s real_flag_lost kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/real_signature_lost"; _mkdir(d);
put(d,"lib.c","int scale(int k, int f) { return k * f; }\n");
put(d,"README","Call `scale(k)` to scale.\n");
kept=TaskOpsSolve(d,"The README shows scale(k) but lib.c defines scale(int k, int f); document scale(int k, int f).",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"lib.c","int scale(int k, int f) { return k * f; }\n");
wrong |= !same(d,"README","Call `scale(k)` to scale.\n");
wrong |= strstr(r.reason,"doc_sync requires verified documentation relation")==NULL;
if(wrong){failures++;mask|=1u<<2;}printf("%s real_signature_lost kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/history_decoy"; _mkdir(d);
put(d,"app.c","int max_retries=3;\nint main(void){return 0;}\n");
put(d,"notes.md","The retired setting was max_replans.\n");
put(d,"old.patch","- int max_replans;\n+ int max_retries;\n");
kept=TaskOpsSolve(d,"notes.md mentions max_replans but app.c uses max_retries. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"app.c","int max_retries=3;\nint main(void){return 0;}\n");
wrong |= !same(d,"notes.md","The retired setting was max_replans.\n");
wrong |= !same(d,"old.patch","- int max_replans;\n+ int max_retries;\n");
wrong |= strstr(r.reason,"doc_sync requires verified documentation relation")==NULL;
if(wrong){failures++;mask|=1u<<3;}printf("%s history_decoy kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/comment_match"; _mkdir(d);
put(d,"app.c","/* max_retries */\nint main(void){return 0;}\n");
put(d,"notes.md","Use max_replans.\n");
kept=TaskOpsSolve(d,"notes.md mentions max_replans but app.c uses max_retries. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"app.c","/* max_retries */\nint main(void){return 0;}\n");
wrong |= !same(d,"notes.md","Use max_replans.\n");
wrong |= strstr(r.reason,"doc_sync requires verified documentation relation")==NULL;
if(wrong){failures++;mask|=1u<<4;}printf("%s comment_match kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/intentional_historical_doc"; _mkdir(d);
put(d,"app.c","int main(void){const char *flag=\"--quiet\";return flag[0]==0;}\n");
put(d,"notes.md","Previous versions accepted --silent; current versions accept --quiet.\n");
kept=TaskOpsSolve(d,"notes.md says --silent but app.c checks --quiet. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"app.c","int main(void){const char *flag=\"--quiet\";return flag[0]==0;}\n");
wrong |= !same(d,"notes.md","Previous versions accepted --silent; current versions accept --quiet.\n");
wrong |= strstr(r.reason,"doc_sync requires verified documentation relation")==NULL;
if(wrong){failures++;mask|=1u<<5;}printf("%s intentional_historical_doc kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/ambiguous"; _mkdir(d);
put(d,"main.c","int main(void) { const char *a = \"--quick\", *b = \"--small\"; return a == b; }\n");
put(d,"notes.md","Use --fast or --tiny.\n");
kept=TaskOpsSolve(d,"notes.md mentions --fast and --tiny; code has --quick and --small.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"main.c","int main(void) { const char *a = \"--quick\", *b = \"--small\"; return a == b; }\n");
wrong |= !same(d,"notes.md","Use --fast or --tiny.\n");
if(wrong){failures++;mask|=1u<<6;}printf("%s ambiguous kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/docs_only"; _mkdir(d);
put(d,"notes.md","Use --silent.\n");
kept=TaskOpsSolve(d,"notes.md says --silent but code checks --quiet. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"notes.md","Use --silent.\n");
if(wrong){failures++;mask|=1u<<7;}printf("%s docs_only kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}
{const char*d="doc_sync_gate_scratch/no_docs"; _mkdir(d);
put(d,"app.c","int main(void){const char *flag=\"--quiet\";return flag[0]==0;}\n");
kept=TaskOpsSolve(d,"notes.md says --silent but app.c checks --quiet. Make the docs match.",&r);
wrong=kept || r.applied || r.verified;
wrong |= !same(d,"app.c","int main(void){const char *flag=\"--quiet\";return flag[0]==0;}\n");
if(wrong){failures++;mask|=1u<<8;}printf("%s no_docs kept=%d applied=%d reason=%s\n",wrong?"FAIL":"PASS",kept,r.applied,r.reason);
}

#ifdef DOC_SYNC_MUTANT
 /* The first six unique plans must fail the guarded oracle, including the
    misleading identifier. Exactly the last three no-plan cells remain safe. */
 if(mask==63u){puts("MUTANT killed exactly six unique doc plans");return 0;}
 printf("UNEXPECTED mutant mask=%u\n",mask);return 1;
#else
 printf("DOC_SYNC_GATE failures=%d\n",failures);return failures?1:0;
#endif
}
