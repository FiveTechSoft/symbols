/* Audit C private-source unit: real implementation, stubbed process boundary.
   Expected values were frozen in agent_git_paths_C_gold.json before coding. */
#include "../src/agent_git.c"
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif
static int runs, failed, calls, mode, exits[2], command_ok;
#define CHECK(expr,id) do { runs++; if (!(expr)) { failed++; printf("[FAIL] %s line=%d\n",id,__LINE__); } else printf("[PASS] %s\n",id); } while (0)
/* mode: 0 patch, 1 changes, 2 launch, 3 timeout, 4/5 truncated stdout/stderr. */
int AgentShellExec(const char *cmd, const char *cwd, uint32_t timeout, SHELL_EXEC_RESULT *out)
{
    const char *want = calls == 0 ? "git apply --check \"dir/p.patch\"" : "git apply --check --reverse \"dir/p.patch\"";
    memset(out, 0, sizeof(*out));
    if (mode != 1 && (strcmp(cmd,want) || !cwd || strcmp(cwd,"test_agent_git_paths_C_scratch") || timeout != 10000)) command_ok=0;
    calls++;
    if (mode == 1) { strcpy(out->stdout_buf,"M\ta.txt\n\nA\tb.txt"); return 1; }
    if (mode == 2) { out->execution_failed=true; return 0; }
    if (mode == 3) out->timed_out=true;
    if (mode == 4) out->stdout_truncated=true;
    if (mode == 5) out->stderr_truncated=true;
    out->exit_code = exits[calls == 1 ? 0 : 1];
    return 1;
}
static int Same(const char *a,const char *b) { return a&&b&&!strcmp(a,b); }
static void Reset(void) { calls=mode=exits[0]=exits[1]=0;command_ok=1; }
static const struct {const char *id,*input,*normalized;int valid;} norms[] = {
    {"plain","p.patch","p.patch",1},
    {"slash","dir/p.patch","dir/p.patch",1},
    {"backslash","dir\134p.patch","dir/p.patch",1},
    {"mixed","dir\134sub/p.patch","dir/sub/p.patch",1},
    {"dot","./p.patch","./p.patch",1},
    {"dot_backslash",".\134p.patch","./p.patch",1},
    {"inner_dash","dir/-p.patch","dir/-p.patch",1},
    {"repeat_slash","dir//p.patch","dir//p.patch",1},
    {"repeat_backslash","dir\134\134p.patch","dir//p.patch",1},
    {"allowed_ascii","Az09._-/p.patch","Az09._-/p.patch",1},
    {"max255","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.patch","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.patch",1},
    {"null",NULL,NULL,0},
    {"empty","",NULL,0},
    {"root_slash","/p.patch",NULL,0},
    {"root_backslash","\134p.patch",NULL,0},
    {"unc","\134\134server\134share\134p.patch",NULL,0},
    {"drive","C:\134p.patch",NULL,0},
    {"drive_relative","C:p.patch",NULL,0},
    {"leading_dash","-p.patch",NULL,0},
    {"parent","../p.patch",NULL,0},
    {"parent_backslash","..\134p.patch",NULL,0},
    {"inner_parent","dir\134..\134p.patch",NULL,0},
    {"double_dot_word","a..b.patch",NULL,0},
    {"space","dir with space/p.patch",NULL,0},
    {"quote","p\042.patch",NULL,0},
    {"single_quote","p'.patch",NULL,0},
    {"semicolon","p.patch;true",NULL,0},
    {"percent","%PATH%.patch",NULL,0},
    {"bang","!p.patch",NULL,0},
    {"ampersand","p&x.patch",NULL,0},
    {"dollar","p$(x).patch",NULL,0},
    {"backtick","p`x`.patch",NULL,0},
    {"newline","p\012.patch",NULL,0},
    {"tab","p\011.patch",NULL,0},
    {"unicode","\303\251.patch",NULL,0},
    {"max256","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.patch",NULL,0},
    {"oversized","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",NULL,0},
};
static const struct { const char *id,*input,*lines[3]; size_t count; } scans[] = {
    {"lf","one\012two\012",{"one","two"},2},
    {"crlf","one\015\012two\015\012",{"one\015","two\015"},2},
    {"no_final_lf","one\012two",{"one","two"},2},
    {"skip_blank","\012\012one\012\012two\012\012",{"one","two"},2},
    {"empty_lines","\012\012",{NULL},0},
    {"empty_buffer","",{NULL},0},
    {"only_cr","\015",{"\015"},1},
};
int main(void)
{
    size_t i,j; char normalized[GIT_BRANCH_MAX],err[GIT_ERROR_MAX];
    for (i=0;i<sizeof(norms)/sizeof(norms[0]);i++) {
        const char *input=norms[i].input;
        char *copy=NULL; int ok, valid;
        if (input) {copy=malloc(strlen(input)+1); if (!copy) return 1; strcpy(copy,input);}
        valid=NormalizePatchPath(copy,normalized);
        ok=(valid==norms[i].valid)&&(!copy||!strcmp(copy,input));
        if (valid) ok=ok&&!strcmp(normalized,norms[i].normalized);
        else {Reset();ok=ok&&AgentGitPatchState(".",copy,err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==0;}
        CHECK(ok,norms[i].id);free(copy);
    }
    for(i=0;i<sizeof(scans)/sizeof(scans[0]);i++) {
        char text[128],*cursor,*line;int ok=1;strcpy(text,scans[i].input);cursor=text;
        for(j=0;(line=NextGitLine(&cursor))!=NULL;j++) {
            if(j>=scans[i].count||strcmp(line,scans[i].lines[j]))ok=0;
        }
        CHECK(ok&&j==scans[i].count,scans[i].id);
    }
    {char a[]="a1\na2",b[]="b1\nb2",outer[]="first,second";char *ca=a,*cb=b;
     int ok=!strcmp(strtok(outer,","),"first");
     ok=ok&&Same(NextGitLine(&ca),"a1")&&Same(NextGitLine(&cb),"b1");
     ok=ok&&Same(NextGitLine(&ca),"a2")&&Same(NextGitLine(&cb),"b2");
     {char *last=strtok(NULL,",");ok=ok&&last&&!strcmp(last,"second");}
     CHECK(ok,"interleaved_scanners");}
    {char outer[]="first,second";GIT_EXPECTED_CHANGE expected[2]={{"a.txt",'M'},{"b.txt",'A'}};
     int ok=!strcmp(strtok(outer,","),"first");Reset();mode=1;
     ok=ok&&AgentGitStagedMatches(".",expected,2,err,sizeof(err))==GIT_CHANGES_MATCH;
     {char *last=strtok(NULL,",");ok=ok&&last&&!strcmp(last,"second");}
     CHECK(ok,"compare_outer_strtok");}
    /* Work in a dedicated CTest working directory; no other fixture shares it. */
    (void)MKDIR("test_agent_git_paths_C_scratch");
    (void)MKDIR("test_agent_git_paths_C_scratch/dir");
    {FILE *f=fopen("test_agent_git_paths_C_scratch/dir/p.patch","wb");if(!f)return 1;if(fputs("stub-only fixture\n",f)<0||fclose(f))return 1;}
    Reset();exits[0]=0;exits[1]=0;
    { char input[]="dir\\p.patch"; CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch",input,err,sizeof(err))==GIT_PATCH_NOT_APPLIED&&calls==1&&command_ok&&!strcmp(input,"dir\\p.patch"),"forward"); }
    Reset();exits[0]=1;exits[1]=0;
    { char input[]="dir\\p.patch"; CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch",input,err,sizeof(err))==GIT_PATCH_ALREADY_APPLIED&&calls==2&&command_ok&&!strcmp(input,"dir\\p.patch"),"reverse"); }
    Reset();exits[0]=1;exits[1]=1;
    { char input[]="dir\\p.patch"; CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch",input,err,sizeof(err))==GIT_PATCH_NO_MATCH&&calls==2&&command_ok&&!strcmp(input,"dir\\p.patch"),"neither"); }
    Reset();exits[0]=128;exits[1]=0;
    { char input[]="dir\\p.patch"; CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch",input,err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==1&&command_ok&&!strcmp(input,"dir\\p.patch"),"parse_error"); }
    Reset();exits[0]=1;exits[1]=128;
    { char input[]="dir\\p.patch"; CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch",input,err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==2&&command_ok&&!strcmp(input,"dir\\p.patch"),"reverse_parse_error"); }
    Reset();mode=2;CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch","dir\\p.patch",err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==1&&command_ok,"launch_failure");
    Reset();mode=3;CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch","dir\\p.patch",err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==1&&command_ok,"timeout");
    Reset();mode=4;CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch","dir\\p.patch",err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==1&&command_ok,"stdout_truncation");
    Reset();mode=5;CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch","dir\\p.patch",err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==1&&command_ok,"stderr_truncation");
    Reset();CHECK(AgentGitPatchState("test_agent_git_paths_C_scratch","dir\\missing.patch",err,sizeof(err))==GIT_PATCH_CHECK_FAILED&&calls==0,"missing_patch");
    if(remove("test_agent_git_paths_C_scratch/dir/p.patch"))return 1;
    printf("C_UNIT runs=%d failed=%d\n",runs,failed);
    return failed?1:0;
}
