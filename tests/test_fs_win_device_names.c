#ifdef _WIN32
#include "fs_write.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Windows writers refuse Win32 device names in every component (found by the
   Windows fuzz: FsCreateFile accepted "NUL"). Every writer entry point is
   tried with each device name, with the name as source and as destination,
   at the workspace root and under "sub". A refused call must leave the
   directory listing (names, kinds, sizes) unchanged. Near-miss names that
   are not devices must still work, so the guard is not wider than the
   documented list: CON PRN AUX NUL COM1-9 LPT1-9, any case, any extension,
   trailing spaces before the extension. */
#define SCRATCH "test_fs_win_device_names_scratch"
static void ck(int x,const char *m)
{
    if(!x){fprintf(stderr,"FAIL %s (gle=%lu)\n",m,GetLastError());exit(1);}
}
static void put(const char *p,const char *v)
{
    FILE *f=fopen(p,"wb");
    ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"fixture");
}
static void listing(char *out,size_t cap,const char *dir)
{
    WIN32_FIND_DATAA fd;char pat[256];HANDLE h;size_t used=0;
    snprintf(pat,sizeof(pat),"%s/*",dir);
    h=FindFirstFileA(pat,&fd);ck(h!=INVALID_HANDLE_VALUE,"list open");
    out[0]=0;
    do{
        if(!strcmp(fd.cFileName,".")||!strcmp(fd.cFileName,".."))continue;
        if(!strcmp(fd.cFileName,".fstxn.lock"))continue;
        used+=(size_t)snprintf(out+used,cap-used,"%s|%lu|%lu;",fd.cFileName,
                (unsigned long)fd.dwFileAttributes,(unsigned long)fd.nFileSizeLow);
        ck(used<cap-1,"list size");
    }while(FindNextFileA(h,&fd));
    FindClose(h);
}
static void same(const char *before,const char *what)
{
    char now[2048],sub[1024];
    listing(now,sizeof(now),SCRATCH);
    (void)sub;
    ck(!strcmp(before,now),what);
}
static void clean(void)
{
    ck(system("if exist " SCRATCH " rmdir /s /q " SCRATCH)==0,"clean");
}
int main(void)
{
    static const char *dev[]={"NUL","nul","Nul.txt","CON","con.txt","PRN","aux",
        "COM1","com9.log","LPT1","lpt9.x","NUL .txt","CON.tar.gz"};
    static const char *ok[]={"NULL","COM0","COM10","LPT0","CONSOLE","nul1",
        "xNUL","aux_","co","a.nul","COMx"};
    FS_READ_ROOT *root;char base[2048],path[128];unsigned refused=0;
    clean();
    ck(CreateDirectoryA(SCRATCH,NULL)&&CreateDirectoryA(SCRATCH "/sub",NULL),"dirs");
    put(SCRATCH "/safe","SAFE");put(SCRATCH "/sub/inner","INNER");
    ck(FsReadOpen(SCRATCH,&root)==FS_READ_OK,"open");
    listing(base,sizeof(base),SCRATCH);
    for(unsigned i=0;i<sizeof(dev)/sizeof(*dev);i++)
        for(int where=0;where<2;where++){
            const char *p;
            snprintf(path,sizeof(path),"%s%s",where?"sub/":"",dev[i]);p=path;
#define REFUSED(call,what) do{ck((call)!=FS_READ_OK,"accepted device name: " what);\
    refused++;same(base,"refused " what " changed the listing");}while(0)
            printf("  device %s\n",p);
            REFUSED(FsCreateFile(root,p,"x",1,0600),"create");
            REFUSED(FsReplaceFile(root,p,"x",1,"y",1),"replace");
            REFUSED(FsRemoveFile(root,p,"x",1),"remove");
            REFUSED(FsMoveFile(root,p,"moved","x",1),"move source");
            REFUSED(FsMoveFile(root,"safe",p,"SAFE",4),"move destination");
            REFUSED(FsCopyFile(root,p,"copied","x",1),"copy source");
            REFUSED(FsCopyFile(root,"safe",p,"SAFE",4),"copy destination");
        }
    /* A device name as a directory component is refused too. */
    REFUSED(FsCreateFile(root,"NUL/x",NULL,0,0600),"create under device dir");
    REFUSED(FsCreateFile(root,"sub/con.d/x",NULL,0,0600),"create under nested device dir");
    /* Near misses are ordinary names: create, then remove with the exact bytes. */
    for(unsigned i=0;i<sizeof(ok)/sizeof(*ok);i++){
        printf("  ordinary %s\n",ok[i]);
        ck(FsCreateFile(root,ok[i],"x",1,0600)==FS_READ_OK,"ordinary name refused: create");
        ck(FsRemoveFile(root,ok[i],"x",1)==FS_READ_OK,"ordinary name refused: remove");
    }
    same(base,"ordinary names left residue");
    FsReadClose(root);clean();
    printf("device names: %u refused calls, %u ordinary names ok\n",refused,
           (unsigned)(sizeof(ok)/sizeof(*ok)));
    return 0;
}
#else
#include <stdio.h>
int main(void){puts("SKIP test_fs_win_device_names: Windows only");return 0;}
#endif
