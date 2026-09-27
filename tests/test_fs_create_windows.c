#ifdef _WIN32
#include "fs_write.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
static void ck(int ok,const char *what){if(!ok){fprintf(stderr,"FAIL %s (%lu)\n",what,GetLastError());exit(1);}}
static void put(const char *path,const char *v)
{FILE *f=fopen(path,"wb");ck(f&&fwrite(v,1,strlen(v),f)==strlen(v)&&fclose(f)==0,"put");}
static void read_exact(FS_READ_ROOT *r,const char *p,const void *expected,size_t len)
{unsigned char *b=NULL;size_t n;FS_READ_META m;
 ck(FsReadFile(r,p,&b,&n,&m)==FS_READ_OK&&n==len&&
    (!len||!memcmp(b,expected,len)),"read exact");free(b);}
/* Teardown must use the extended-length namespace: the 240-character leaf
   is created and read through held handles, but a normal DeleteFileA path
   exceeds MAX_PATH once the checkout prefix is included. */
static wchar_t *cleanup_path(const char *relative)
{
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,relative,-1,NULL,0);
 wchar_t *wide,*absolute,*extended;DWORD needed,got;
 size_t length;
 if(count<=0)return NULL;
 wide=(wchar_t*)malloc((size_t)count*sizeof(*wide));
 if(!wide)return NULL;
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,relative,-1,wide,count)){
  free(wide);return NULL;
 }
 needed=GetFullPathNameW(wide,0,NULL,NULL);
 if(!needed||needed>32760){free(wide);return NULL;}
 absolute=(wchar_t*)malloc((size_t)needed*sizeof(*absolute));
 if(!absolute){free(wide);return NULL;}
 got=GetFullPathNameW(wide,needed,absolute,NULL);free(wide);
 if(!got||got>=needed){free(absolute);return NULL;}
 length=wcslen(absolute);
 /* These fixtures use a drive-letter checkout, not a UNC working directory. */
 if(length<3||absolute[1]!=L':'||absolute[2]!=L'\\'){
  free(absolute);return NULL;
 }
 extended=(wchar_t*)malloc((length+5)*sizeof(*extended));
 if(!extended){free(absolute);return NULL;}
 wcscpy(extended,L"\\\\?\\");wcscat(extended,absolute);
 free(absolute);return extended;
}
static int cleanup_file(const char *path)
{wchar_t *p=cleanup_path(path);BOOL ok;
 if(!p)return 0;ok=DeleteFileW(p);free(p);return ok!=0;}
static int cleanup_dir(const char *path)
{wchar_t *p=cleanup_path(path);BOOL ok;
 if(!p)return 0;ok=RemoveDirectoryW(p);free(p);return ok!=0;}
int main(void)
{
 FS_READ_ROOT *r;FS_READ_META m;unsigned char binary[]={0,1,255,10};
 HANDLE lock;char longname[244],longpath[256];
 ck(_mkdir("test_fs_wincreate_scratch")==0,"root");
 ck(_mkdir("test_fs_wincreate_scratch\\inside")==0,"inside");
 put("test_fs_wincreate_scratch\\existing","original");
 put("test_fs_wincreate_outside","outside");
 ck(FsReadOpen("test_fs_wincreate_scratch",&r)==FS_READ_OK,"open root");
 {FS_READ_STATUS status=FsCreateFile(r,"inside/new","created",7,0666);
  if(status!=FS_READ_OK)fprintf(stderr,"create FS_READ_STATUS=%d\n",(int)status);
  ck(status==FS_READ_OK,"create");}
 read_exact(r,"inside/new","created",7);
 ck(FsCreateFile(r,"inside/NEW","bad",3,0666)==FS_READ_DENIED,"case alias no overwrite");
 read_exact(r,"inside/new","created",7);
 ck(FsCreateFile(r,"existing","bad",3,0666)==FS_READ_DENIED,"existing name");
 read_exact(r,"existing","original",8);
 ck(FsCreateFile(r,"empty","",0,0666)==FS_READ_OK,"empty");
 read_exact(r,"empty","",0);
 ck(FsCreateFile(r,"binary",binary,sizeof(binary),0666)==FS_READ_OK,"binary");
 read_exact(r,"binary",binary,sizeof(binary));
 ck(FsCreateFile(r,"readonly","r",1,0444)==FS_READ_OK,"readonly create");
 ck(FsReadStat(r,"readonly",&m)==FS_READ_OK&&m.mode==0444,"readonly metadata");
 read_exact(r,"readonly","r",1);
 ck(SetFileAttributesA("test_fs_wincreate_scratch\\readonly",FILE_ATTRIBUTE_NORMAL),
    "reset readonly fixture");
 ck(FsCreateFile(r,"badsize",binary,1024u*1024u+1,0666)==FS_READ_INVALID,"length limit");
 ck(FsReadStat(r,"badsize",&m)==FS_READ_MISSING,"no oversized file");
 ck(FsCreateFile(r,"inside","bad",3,0666)==FS_READ_DENIED,"directory leaf");
 ck(FsCreateFile(r,"../test_fs_wincreate_outside","bad",3,0666)==FS_READ_INVALID,"parent escape");
 ck(FsCreateFile(r,"C:/escape","bad",3,0666)==FS_READ_INVALID,"drive escape");
 ck(FsCreateFile(r,"inside\\escape","bad",3,0666)==FS_READ_INVALID,"separator escape");
 ck(FsCreateFile(r,".fstxn.lock","bad",3,0666)==FS_READ_DENIED,"control name");
 ck(FsCreateFile(r,".FSTXN.LOCK","bad",3,0666)==FS_READ_DENIED,"case-folded control name");
 ck(FsCreateFile(r,"inside/.FsRp-evil","bad",3,0666)==FS_READ_DENIED,"nested stage alias");
 ck(FsCreateFile(r,"inside/trailing.","bad",3,0666)==FS_READ_INVALID,"trailing dot alias");
 ck(FsCreateFile(r,"inside/trailing ","bad",3,0666)==FS_READ_INVALID,"trailing space alias");
 ck(FsCreateFile(r,"inside/.fstxn.pcommit","bad",3,0666)==FS_READ_DENIED,
    "nested control marker");
 /* A-G process-termination crash matrix runs separately below. */
 ck(FsCreateRecover(r)==FS_READ_OK,"empty recovery");
 _putenv_s("FS_WIN_TEST_SIMULATE_NON_NTFS","1");
 ck(FsCreateFile(r,"nonntfs","data",4,0666)==FS_READ_UNSUPPORTED,
    "simulated capability rejection (not a real non-NTFS volume)");
 _putenv_s("FS_WIN_TEST_SIMULATE_NON_NTFS","");
 memset(longname,'a',240);longname[240]=0;
 snprintf(longpath,sizeof(longpath),"inside/%s",longname);
 ck(FsCreateFile(r,longpath,"long",4,0666)==FS_READ_OK,"long component");
 read_exact(r,longpath,"long",4);
 lock=CreateFileA("test_fs_wincreate_scratch\\inside\\new",GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
 ck(lock!=INVALID_HANDLE_VALUE,"lock existing");
 ck(FsCreateFile(r,"inside/new","bad",3,0666)!=FS_READ_OK,"locked existing");
 CloseHandle(lock);read_exact(r,"inside/new","created",7);
 /* A junction is a reparse parent and requires no symlink privilege. */
 {char absolute[MAX_PATH],cmd[2*MAX_PATH+128];DWORD n;
  n=GetFullPathNameA("test_fs_wincreate_scratch\\inside",MAX_PATH,absolute,NULL);
  ck(n>0&&n<MAX_PATH,"junction target path");
  ck(snprintf(cmd,sizeof(cmd),"cmd /D /C mklink /J \"test_fs_wincreate_scratch\\dirlink\" \"%s\" >NUL",absolute)>0,
     "junction command");
  ck(system(cmd)==0,"junction fixture");}
 ck(FsCreateFile(r,"dirlink/nope","bad",3,0666)!=FS_READ_OK,"reparse parent");
 ck(FsReadStat(r,"inside/nope",&m)==FS_READ_MISSING,"no reparse traversal");
 ck(cleanup_dir("test_fs_wincreate_scratch\\dirlink"),"unlink junction");
 /* CreateSymbolicLink may require a local privilege, so exercise it if the
    runner permits it; the junction above is the unconditional reparse test. */
 if(CreateSymbolicLinkA("test_fs_wincreate_scratch\\symparent","inside",
       SYMBOLIC_LINK_FLAG_DIRECTORY|0x2 /* ALLOW_UNPRIVILEGED_CREATE */)){
  ck(FsCreateFile(r,"symparent/nope","bad",3,0666)!=FS_READ_OK,
     "directory symlink parent");
  ck(cleanup_dir("test_fs_wincreate_scratch\\symparent"),"remove symlink");
 }
 if(CreateSymbolicLinkA("test_fs_wincreate_scratch\\symleaf",
       "..\\test_fs_wincreate_outside",0x2)){
  ck(FsCreateFile(r,"symleaf","bad",3,0666)==FS_READ_DENIED,"symlink leaf");
  ck(cleanup_file("test_fs_wincreate_scratch\\symleaf"),"remove leaf symlink");
 }
 /* Existing leaf must never be opened or overwritten. */
 ck(FsCreateFile(r,"existing","bad",3,0666)==FS_READ_DENIED,"leaf no overwrite");
 ck(MoveFileA("test_fs_wincreate_scratch","test_fs_wincreate_moved"),"move held root");
 ck(_mkdir("test_fs_wincreate_scratch")==0,"replacement root");
 ck(FsCreateFile(r,"held","safe",4,0666)==FS_READ_OK,"create held root");
 read_exact(r,"held","safe",4);
 ck(GetFileAttributesA("test_fs_wincreate_scratch\\held")==INVALID_FILE_ATTRIBUTES,
    "replacement root untouched");
 ck(cleanup_dir("test_fs_wincreate_scratch"),"remove replacement root");
 ck(MoveFileA("test_fs_wincreate_moved","test_fs_wincreate_scratch"),"restore root");
 ck(FsCreateRecover(r)==FS_READ_OK,"Windows recovery idempotent");
 FsReadClose(r);
 ck(cleanup_file("test_fs_wincreate_scratch\\inside\\new"),"cleanup new");
 {char full[300];snprintf(full,sizeof(full),"test_fs_wincreate_scratch\\inside\\%s",longname);
  ck(cleanup_file(full),"cleanup long");}
 ck(cleanup_dir("test_fs_wincreate_scratch\\inside"),"cleanup inside");
 ck(cleanup_file("test_fs_wincreate_scratch\\existing"),"cleanup existing");
 ck(cleanup_file("test_fs_wincreate_scratch\\empty"),"cleanup empty");
 ck(cleanup_file("test_fs_wincreate_scratch\\binary"),"cleanup binary");

 ck(cleanup_file("test_fs_wincreate_scratch\\readonly"),"cleanup readonly");
 ck(cleanup_file("test_fs_wincreate_scratch\\held"),"cleanup held");
 ck(cleanup_file("test_fs_wincreate_scratch\\.fstxn.lock"),"cleanup lock");
 ck(cleanup_dir("test_fs_wincreate_scratch"),"cleanup root");
 ck(cleanup_file("test_fs_wincreate_outside"),"cleanup outside");
 puts("Windows handle-relative create passed");return 0;
}
#else
int main(void){return 0;}
#endif
