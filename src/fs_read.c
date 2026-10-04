#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_read.h"
#include "fs_write.h"
#include "fs_batch.h"
#include "fs_remove.h"
#include "fs_move.h"
#include "fs_replace.h"
#include "fs_manifest.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define FS_READ_MAX (1024U * 1024U)

static FS_READ_STATUS error_status(void)
{
    if (errno == ENOENT) return FS_READ_MISSING;
    if (errno == EACCES || errno == EPERM || errno == ELOOP) return FS_READ_DENIED;
    return FS_READ_IO;
}
/* Test-only mutants (never defined in production targets): MC2 lets a colon
   through this validator. See tests/test_fs_win_aliases.c. */
#if defined(FS_ALIAS_MUTANT) && FS_ALIAS_MUTANT==2
#define VR_COLON 0
#else
#define VR_COLON (*p == ':')
#endif
static int valid_relative(const char *p, int allow_empty)
{
    const char *start;
    if (!p || (!*p && !allow_empty) || *p == '/' || *p == '\\') return 0;
    if (!*p) return 1;
    for (start=p; *p; p++) {
        if (*p == '\\' || VR_COLON || (unsigned char)*p < 32) return 0;
        if (*p == '/') {
            size_t n=(size_t)(p-start);
            if (!n || (n==1 && *start=='.') ||
                (n==2 && start[0]=='.' && start[1]=='.')) return 0;
            start=p+1;
        }
    }
    return p!=start && !((p-start)==1 && *start=='.') &&
           !((p-start)==2 && start[0]=='.' && start[1]=='.');
}
static void scan_bytes(const unsigned char *p,size_t n,FS_READ_META *m)
{
    size_t crlf=0, lf=0, cr=0;
    m->binary=memchr(p,0,n)!=NULL;
    for (size_t i=0;i<n;i++) {
        if (p[i]=='\r') { if (i+1<n && p[i+1]=='\n') {crlf++;i++;} else cr++; }
        else if (p[i]=='\n') lf++;
    }
    m->newline=(crlf && !lf && !cr)?FS_NEWLINE_CRLF:
               (lf && !crlf && !cr)?FS_NEWLINE_LF:
               (lf || crlf || cr)?FS_NEWLINE_MIXED:FS_NEWLINE_NONE;
}
static int entry_cmp(const void *a,const void *b)
{ return strcmp(((const FS_READ_ENTRY*)a)->name,((const FS_READ_ENTRY*)b)->name); }
void FsReadFreeList(FS_READ_ENTRY *e,size_t n)
{ if (!e) return; for(size_t i=0;i<n;i++) free(e[i].name); free(e); }

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <wchar.h>
#ifndef FILE_OPEN_REPARSE_POINT
#define FILE_OPEN_REPARSE_POINT 0x00200000
#endif
#ifndef FILE_CREATE
#define FILE_CREATE 0x00000002
#endif
#ifndef FILE_NON_DIRECTORY_FILE
#define FILE_NON_DIRECTORY_FILE 0x00000040
#endif
#ifndef FILE_DIRECTORY_FILE
#define FILE_DIRECTORY_FILE 0x00000001
#endif
#ifndef FILE_SYNCHRONOUS_IO_NONALERT
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020
#endif
#ifndef FILE_OPEN
#define FILE_OPEN 0x00000001
#endif
#ifndef NT_SUCCESS
#define NT_SUCCESS(s) ((NTSTATUS)(s) >= 0)
#endif
struct FS_READ_ROOT { HANDLE handle; };
typedef NTSTATUS (NTAPI *FS_NT_CREATE)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,
    PIO_STATUS_BLOCK,PLARGE_INTEGER,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG);
static FS_READ_STATUS win_error(DWORD e)
{
    if(e==ERROR_FILE_NOT_FOUND || e==ERROR_PATH_NOT_FOUND)return FS_READ_MISSING;
    if(e==ERROR_ACCESS_DENIED || e==ERROR_CANT_ACCESS_FILE)return FS_READ_DENIED;
    return FS_READ_IO;
}
static wchar_t *win_wide(const char *s)
{
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);
    wchar_t *w;if(n<=0)return NULL;
    w=(wchar_t*)malloc((size_t)n*sizeof(wchar_t));if(!w)return NULL;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,w,n)){free(w);return NULL;}
    return w;
}
static FS_READ_STATUS win_meta(HANDLE h,FS_READ_META *m)
{
    BY_HANDLE_FILE_INFORMATION i;
    FILE_STANDARD_INFO std;
    if(!GetFileInformationByHandle(h,&i))return win_error(GetLastError());
    if(!GetFileInformationByHandleEx(h,FileStandardInfo,&std,sizeof(std)))return FS_READ_IO;
#ifndef FS_REPARSE_MUTANT
    if(i.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)return FS_READ_DENIED;
#endif /* FS_REPARSE_MUTANT: test-only build MC1 drops this check; never defined in production targets */
    if(!!(i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=!!std.Directory)return FS_READ_DENIED;
    if(i.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)return FS_READ_UNSUPPORTED;
    m->kind=std.Directory?FS_KIND_DIR:FS_KIND_FILE;
    m->size=((uint64_t)i.nFileSizeHigh<<32)|i.nFileSizeLow;
    m->mode=(i.dwFileAttributes & FILE_ATTRIBUTE_READONLY)?0444:0644;
    m->binary=-1;m->newline=FS_NEWLINE_NONE;return FS_READ_OK;
}
/* Each component is resolved by the kernel relative to the currently held
   directory handle. A reparse point is opened itself and rejected, never
   traversed. No validation/reopen-by-path gap is possible. */
static FS_READ_STATUS win_open(const FS_READ_ROOT *r,const char *rel,HANDLE *out)
{
    FS_NT_CREATE create;HMODULE dll;wchar_t *w,*part,*next;
    HANDLE current=INVALID_HANDLE_VALUE,child=INVALID_HANDLE_VALUE;
    FS_READ_STATUS s=FS_READ_IO;
    if(!valid_relative(rel,1))return FS_READ_INVALID;
    if(!*rel){
        if(!DuplicateHandle(GetCurrentProcess(),r->handle,GetCurrentProcess(),out,
                            0,FALSE,DUPLICATE_SAME_ACCESS))return FS_READ_IO;
        return FS_READ_OK;
    }
    dll=GetModuleHandleW(L"ntdll.dll");
    if(!dll)return FS_READ_UNSUPPORTED;
    create=(FS_NT_CREATE)(void*)GetProcAddress(dll,"NtCreateFile");
    if(!create)return FS_READ_UNSUPPORTED;
    w=win_wide(rel);if(!w)return FS_READ_INVALID;
    current=r->handle;part=w;
    while(*part){
        UNICODE_STRING name;OBJECT_ATTRIBUTES attrs;IO_STATUS_BLOCK ios;
        FS_READ_META meta;NTSTATUS status;size_t length;
        next=wcschr(part,L'/');if(next)*next++=0;
        length=wcslen(part);if(length>32767){s=FS_READ_INVALID;goto done;}
        name.Buffer=part;name.Length=(USHORT)(length*sizeof(wchar_t));
        name.MaximumLength=name.Length;
        memset(&attrs,0,sizeof(attrs));
        attrs.Length=sizeof(attrs);attrs.RootDirectory=current;
        attrs.ObjectName=&name;attrs.Attributes=OBJ_CASE_INSENSITIVE;
        /* Bit 1 is FILE_READ_DATA for files, FILE_LIST_DIRECTORY for dirs. */
        status=create(&child,FILE_READ_DATA|FILE_READ_ATTRIBUTES|SYNCHRONIZE,
                      &attrs,&ios,NULL,FILE_ATTRIBUTE_NORMAL,
                      FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,FILE_OPEN,
                      FILE_OPEN_REPARSE_POINT|FILE_SYNCHRONOUS_IO_NONALERT|
                          (next?FILE_DIRECTORY_FILE:0),NULL,0);
        if(!NT_SUCCESS(status)){
            /* STATUS_OBJECT_NAME_NOT_FOUND / STATUS_OBJECT_PATH_NOT_FOUND. */
            s=(status==(NTSTATUS)0xC0000034L || status==(NTSTATUS)0xC000003AL)?
                FS_READ_MISSING:FS_READ_DENIED;
            goto done;
        }
        s=win_meta(child,&meta);if(s!=FS_READ_OK)goto done;
        if(next && meta.kind!=FS_KIND_DIR){s=FS_READ_UNSUPPORTED;goto done;}
        if(current!=r->handle)CloseHandle(current);
        current=child;child=INVALID_HANDLE_VALUE;
        if(!next)break;
        part=next;
    }
    *out=current;current=INVALID_HANDLE_VALUE;s=FS_READ_OK;
done:
    if(child!=INVALID_HANDLE_VALUE)CloseHandle(child);
    if(current!=INVALID_HANDLE_VALUE && current!=r->handle)CloseHandle(current);
    free(w);return s;
}
FS_READ_STATUS FsReadOpen(const char *root,FS_READ_ROOT **out)
{
    wchar_t *w;HANDLE h;FS_READ_META m;FS_READ_ROOT *r;FS_READ_STATUS s;
    if(!root||!out||!*root)return FS_READ_INVALID;
    *out=NULL;w=win_wide(root);if(!w)return FS_READ_INVALID;
    h=CreateFileW(w,FILE_READ_ATTRIBUTES|FILE_LIST_DIRECTORY,
                  FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                  NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    free(w);if(h==INVALID_HANDLE_VALUE)return win_error(GetLastError());
    s=win_meta(h,&m);if(s!=FS_READ_OK || m.kind!=FS_KIND_DIR){
        CloseHandle(h);return s!=FS_READ_OK?s:FS_READ_UNSUPPORTED;
    }
    r=(FS_READ_ROOT*)malloc(sizeof(*r));if(!r){CloseHandle(h);return FS_READ_IO;}
    r->handle=h;*out=r;return FS_READ_OK;
}
void FsReadClose(FS_READ_ROOT *r){if(r){CloseHandle(r->handle);free(r);}}
FS_READ_STATUS FsReadStat(const FS_READ_ROOT *r,const char *rel,FS_READ_META *m)
{
    HANDLE h;FS_READ_STATUS s;if(!r||!m)return FS_READ_INVALID;
    s=win_open(r,rel,&h);if(s!=FS_READ_OK)return s;
    s=win_meta(h,m);CloseHandle(h);return s;
}
FS_READ_STATUS FsReadFile(const FS_READ_ROOT *r,const char *rel,unsigned char **b,size_t *n,FS_READ_META *m)
{
    HANDLE h;FS_READ_STATUS s;unsigned char *p;DWORD got,extra;unsigned char one;
    if(!r||!b||!n||!m||!valid_relative(rel,0))return FS_READ_INVALID;
    *b=NULL;*n=0;s=win_open(r,rel,&h);if(s!=FS_READ_OK)return s;
    s=win_meta(h,m);if(s!=FS_READ_OK){CloseHandle(h);return s;}
    if(m->kind!=FS_KIND_FILE || m->size>FS_READ_MAX){CloseHandle(h);return FS_READ_UNSUPPORTED;}
    p=(unsigned char*)malloc((size_t)m->size+1);if(!p){CloseHandle(h);return FS_READ_IO;}
    if(!ReadFile(h,p,(DWORD)m->size,&got,NULL) || got!=m->size ||
       !ReadFile(h,&one,1,&extra,NULL) || extra!=0){free(p);CloseHandle(h);return FS_READ_IO;}
    CloseHandle(h);*b=p;*n=got;scan_bytes(p,*n,m);return FS_READ_OK;
}
FS_READ_STATUS FsReadList(const FS_READ_ROOT *r,const char *rel,FS_READ_ENTRY **entries,size_t *count)
{
    HANDLE h;FS_READ_META m;FS_READ_STATUS s;
    union { unsigned long long aligned; unsigned char bytes[65536]; } buffer;
    FS_READ_ENTRY *list=NULL;size_t n=0,cap=0;
    if(!r||!entries||!count)return FS_READ_INVALID;
    *entries=NULL;*count=0;s=win_open(r,rel,&h);if(s!=FS_READ_OK)return s;
    s=win_meta(h,&m);if(s!=FS_READ_OK||m.kind!=FS_KIND_DIR){CloseHandle(h);return s!=FS_READ_OK?s:FS_READ_UNSUPPORTED;}
    while(1){
        FILE_ID_BOTH_DIR_INFO *item;
        if(!GetFileInformationByHandleEx(h,FileIdBothDirectoryInfo,buffer.bytes,sizeof(buffer.bytes))){
            DWORD e=GetLastError();if(e!=ERROR_NO_MORE_FILES){s=win_error(e);goto done;}break;
        }
        item=(FILE_ID_BOTH_DIR_INFO*)buffer.bytes;
        while(1){
            size_t offset=(size_t)((unsigned char*)item-buffer.bytes);
            if(offset>sizeof(buffer.bytes)-sizeof(*item) ||
               item->FileNameLength>sizeof(buffer.bytes)-offset-
                                      offsetof(FILE_ID_BOTH_DIR_INFO,FileName)){s=FS_READ_IO;goto done;}
            int wn=(int)(item->FileNameLength/sizeof(wchar_t));
            if(wn>0 && !(wn==1 && item->FileName[0]==L'.') &&
               !(wn==2 && item->FileName[0]==L'.' && item->FileName[1]==L'.')){
                int k;FS_READ_ENTRY *newlist;
                if(item->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT){s=FS_READ_DENIED;goto done;}
                k=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,item->FileName,wn,NULL,0,NULL,NULL);
                if(k<=0){s=FS_READ_IO;goto done;}
                if(n==cap){size_t nc=cap?cap*2:16;
                    newlist=(FS_READ_ENTRY*)realloc(list,nc*sizeof(*list));
                    if(!newlist){s=FS_READ_IO;goto done;}list=newlist;cap=nc;}
                list[n].name=(char*)malloc((size_t)k+1);if(!list[n].name){s=FS_READ_IO;goto done;}
                if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,item->FileName,wn,
                                        list[n].name,k,NULL,NULL)){free(list[n].name);s=FS_READ_IO;goto done;}
                list[n].name[k]=0;
                list[n].kind=(item->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)?FS_KIND_DIR:FS_KIND_FILE;
                n++;
            }
            if(!item->NextEntryOffset)break;
            if(item->NextEntryOffset>sizeof(buffer.bytes)-offset ||
               item->NextEntryOffset<sizeof(*item)){s=FS_READ_IO;goto done;}
            item=(FILE_ID_BOTH_DIR_INFO*)((unsigned char*)item+item->NextEntryOffset);
        }
    }
    qsort(list,n,sizeof(*list),entry_cmp);*entries=list;*count=n;list=NULL;n=0;s=FS_READ_OK;
done:FsReadFreeList(list,n);CloseHandle(h);return s;
}
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/file.h>
struct FS_READ_ROOT { int fd; };
FS_READ_STATUS FsReadOpen(const char *root,FS_READ_ROOT **out)
{
    int fd;struct stat st;FS_READ_ROOT *r;
    if(!root||!out||!*root)return FS_READ_INVALID;
    *out=NULL;fd=open(root,O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return error_status();
    if(fstat(fd,&st)<0||!S_ISDIR(st.st_mode)){close(fd);return FS_READ_DENIED;}
    r=(FS_READ_ROOT*)malloc(sizeof(*r));if(!r){close(fd);return FS_READ_IO;}
    r->fd=fd;*out=r;return FS_READ_OK;
}
void FsReadClose(FS_READ_ROOT *r){if(r){close(r->fd);free(r);}}
/* Per-component open of a parent path. The mutant (test_fs_race_parent_mc) drops
   O_NOFOLLOW here only; production always has it. */
#ifdef FS_PARENT_FOLLOW_MUTANT
#define FS_PART_NOFOLLOW 0
#else
#define FS_PART_NOFOLLOW O_NOFOLLOW
#endif

static FS_READ_STATUS posix_open(const FS_READ_ROOT *r,const char *rel,int *out)
{
    int fd;char *copy,*part,*next;
    if(!valid_relative(rel,1))return FS_READ_INVALID;
    fd=dup(r->fd);if(fd<0)return error_status();
    if(!*rel){*out=fd;return FS_READ_OK;}
    copy=(char*)malloc(strlen(rel)+1);if(!copy){close(fd);return FS_READ_IO;}
    strcpy(copy,rel);part=copy;
    while (*part) {
        int child;
        next=strchr(part,'/');if(next)*next++=0;
        child=openat(fd,part,O_RDONLY|O_NONBLOCK|O_CLOEXEC|FS_PART_NOFOLLOW|(next?O_DIRECTORY:0));
        if(child<0){FS_READ_STATUS s=error_status();free(copy);close(fd);return s;}
        close(fd);fd=child;if(!next)break;part=next;
    }
    free(copy);*out=fd;return FS_READ_OK;
}
static FS_READ_STATUS posix_meta(int fd,FS_READ_META *m)
{
    struct stat st;if(fstat(fd,&st)<0)return error_status();
    if(S_ISREG(st.st_mode))m->kind=FS_KIND_FILE;
    else if(S_ISDIR(st.st_mode))m->kind=FS_KIND_DIR;
    else return FS_READ_UNSUPPORTED;
    m->size=(uint64_t)st.st_size;m->mode=(unsigned)(st.st_mode&0777);
    m->binary=-1;m->newline=FS_NEWLINE_NONE;return FS_READ_OK;
}
FS_READ_STATUS FsReadStat(const FS_READ_ROOT *r,const char *rel,FS_READ_META *m)
{
    int fd;FS_READ_STATUS s;if(!r||!m)return FS_READ_INVALID;
    s=posix_open(r,rel,&fd);if(s)return s;
    s=posix_meta(fd,m);close(fd);return s;
}
FS_READ_STATUS FsReadFile(const FS_READ_ROOT *r,const char *rel,unsigned char **b,size_t *n,FS_READ_META *m)
{
    int fd;FS_READ_STATUS s;unsigned char *p;size_t used=0;
    if(!r||!b||!n||!m)return FS_READ_INVALID;
    *b=NULL;*n=0;s=posix_open(r,rel,&fd);if(s)return s;
    s=posix_meta(fd,m);if(s){close(fd);return s;}
    if(m->kind!=FS_KIND_FILE||m->size>FS_READ_MAX){close(fd);return FS_READ_UNSUPPORTED;}
    p=(unsigned char*)malloc((size_t)m->size+1);if(!p){close(fd);return FS_READ_IO;}
    while(used<(size_t)m->size){ssize_t got=read(fd,p+used,(size_t)m->size-used);
        if(got<=0){free(p);close(fd);return FS_READ_IO;}used+=(size_t)got;}
    /* If an attacker grows a file during read, refuse a partial snapshot. */
    unsigned char extra;if(read(fd,&extra,1)!=0){free(p);close(fd);return FS_READ_IO;}
    close(fd);*b=p;*n=used;scan_bytes(p,used,m);return FS_READ_OK;
}
FS_READ_STATUS FsReadList(const FS_READ_ROOT *r,const char *rel,FS_READ_ENTRY **entries,size_t *count)
{
    int fd;DIR *dir;FS_READ_META m;FS_READ_STATUS s;
    FS_READ_ENTRY *list=NULL;size_t n=0,cap=0;
    if(!r||!entries||!count)return FS_READ_INVALID;
    *entries=NULL;*count=0;s=posix_open(r,rel,&fd);if(s)return s;
    s=posix_meta(fd,&m);if(s||m.kind!=FS_KIND_DIR){close(fd);return s?s:FS_READ_UNSUPPORTED;}
    dir=fdopendir(fd);if(!dir){close(fd);return FS_READ_IO;}
    errno=0;struct dirent *ent;
    while((ent=readdir(dir))){struct stat st;
        if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,".."))continue;
        if(fstatat(fd,ent->d_name,&st,AT_SYMLINK_NOFOLLOW)<0){s=error_status();goto done;}
        if(S_ISLNK(st.st_mode)){s=FS_READ_DENIED;goto done;}
        if(!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)){s=FS_READ_UNSUPPORTED;goto done;}
        if(n==cap){size_t next=cap?cap*2:16;FS_READ_ENTRY *newlist=(FS_READ_ENTRY*)realloc(list,next*sizeof(*list));
            if(!newlist){s=FS_READ_IO;goto done;}list=newlist;cap=next;}
        list[n].name=(char*)malloc(strlen(ent->d_name)+1);
        if(!list[n].name){s=FS_READ_IO;goto done;}
        strcpy(list[n].name,ent->d_name);
        list[n].kind=S_ISDIR(st.st_mode)?FS_KIND_DIR:FS_KIND_FILE;
        n++;errno=0;
    }
    if(errno){s=error_status();goto done;}
    qsort(list,n,sizeof(*list),entry_cmp);*entries=list;*count=n;list=NULL;n=0;s=FS_READ_OK;
done:FsReadFreeList(list,n);closedir(dir);return s;
}
#endif

/* Create is separate from the read API contract. The parent handle is
   reacquired for each call; a dry-run manifest never grants write authority. */
#ifdef _WIN32
#include "fs_create_win.inc"
FS_READ_STATUS FsBatchCreate(const FS_READ_ROOT *r,const FS_BATCH_CREATE *e,size_t n)
{ return wb_batch_create(r,e,n); }
FS_READ_STATUS FsBatchRecover(const FS_READ_ROOT *r)
{ return wb_batch_recover(r); }
FS_READ_STATUS FsBatchReplace(const FS_READ_ROOT *r,const FS_BATCH_REPLACE *e,size_t n)
{ return wb_batch_replace(r,e,n); }
#else
/* The lock is an inode under the held root, not a path reopened by name.
   External processes that unlink or ignore it are outside cooperative isolation. */
static FS_READ_STATUS lock_workspace(const FS_READ_ROOT *r,int *lockfd)
{
    int fd;struct stat held,named;
    fd=openat(r->fd,".fstxn.lock",O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return error_status();
    if(fstat(fd,&held)<0||!S_ISREG(held.st_mode)||held.st_nlink!=1||
       (held.st_mode&077)!=0){close(fd);return FS_READ_DENIED;}
    if(flock(fd,LOCK_EX)<0){close(fd);return FS_READ_IO;}
    if(fstatat(r->fd,".fstxn.lock",&named,AT_SYMLINK_NOFOLLOW)<0||
       named.st_dev!=held.st_dev||named.st_ino!=held.st_ino||
       !S_ISREG(named.st_mode)||named.st_nlink!=1){
        flock(fd,LOCK_UN);close(fd);return FS_READ_DENIED;
    }
    *lockfd=fd;return FS_READ_OK;
}
static void unlock_workspace(int fd)
{ if(fd>=0){(void)flock(fd,LOCK_UN);close(fd);} }
#define FS_INTENT_NAME ".fstxn.intent"
#define FS_BATCH_NAME ".fstxn.batch"
#define FS_BATCH_COMMIT ".fstxn.commit"
#define FS_REMOVE_NAME ".fstxn.remove"
#define FS_REMOVE_COMMIT ".fstxn.rcommit"
#define FS_MOVE_NAME ".fstxn.move"
#define FS_MOVE_COMMIT ".fstxn.mcommit"
#define FS_REPLACE_NAME ".fstxn.replace"
#define FS_REPLACE_COMMIT ".fstxn.pcommit"
#define FS_INTENT_MAGIC 0x46535458u
#define FS_INTENT_MAX_PATH 1024
/* Fixed-size versioned record, checked in full before any cleanup. Same-host
   crash replay only, not an interchange format or protection from hostile writes. */
typedef struct {
    uint32_t magic,version;
    uint64_t dev,ino;
    char target[FS_INTENT_MAX_PATH];
    char stage[48];
} FS_CREATE_INTENT;
static void crash_point(int step)
{
#ifdef FS_CREATE_TEST_CRASH
    const char *v=getenv("FS_CREATE_TEST_CRASH");
    if(v && atoi(v)==step)_exit(90+step);
#else
    (void)step;
#endif
}
static int intent_valid(const FS_CREATE_INTENT *i)
{
    return i->magic==FS_INTENT_MAGIC && i->version==1 &&
        memchr(i->target,0,sizeof(i->target)) &&
        memchr(i->stage,0,sizeof(i->stage)) &&
        valid_relative(i->target,0) &&
        strncmp(i->stage,".fst-",5)==0 && strlen(i->stage)==37 &&
        strspn(i->stage+5,"0123456789abcdef")==32 &&
        strcmp(i->target,".fstxn.lock") &&
        strcmp(i->target,FS_INTENT_NAME) &&
        strcmp(i->target,FS_BATCH_NAME) &&
        strcmp(i->target,FS_BATCH_COMMIT) &&
        strcmp(i->target,FS_REMOVE_NAME) &&
        strcmp(i->target,FS_REMOVE_COMMIT) &&
        strcmp(i->target,FS_MOVE_NAME) &&
        strcmp(i->target,FS_MOVE_COMMIT) &&
        strcmp(i->target,FS_REPLACE_NAME) &&
        strcmp(i->target,FS_REPLACE_COMMIT) &&
        strcmp(i->target,".fstxn") &&
        strncmp(i->target,".fstxn/",7) &&
        strncmp(i->target,".fst-",5) &&
        strncmp(i->target,".fstxn-",7);
}
static FS_READ_STATUS batch_state(const FS_READ_ROOT *r,int *batch,int *commit);
static FS_READ_STATUS remove_state(const FS_READ_ROOT *r,int *intent,int *commit);
static FS_READ_STATUS move_state(const FS_READ_ROOT *r,int *intent,int *commit);
static FS_READ_STATUS replace_state(const FS_READ_ROOT *r,int *intent,int *commit);
static FS_READ_STATUS pending_intent(const FS_READ_ROOT *r,int *present)
{
    struct stat st;
    if(fstatat(r->fd,FS_INTENT_NAME,&st,AT_SYMLINK_NOFOLLOW)==0){
        *present=1;return FS_READ_OK;
    }
    if(errno==ENOENT){*present=0;return FS_READ_OK;}
    return error_status();
}
static int durable_remove(int dir,const char *name)
{
    return unlinkat(dir,name,0)==0 && fsync(dir)==0;
}
/* Caller holds the workspace lock. Validate every name and inode before
   unlinking either record or stage. A foreign target is never removed. */
static FS_READ_STATUS recover_locked(const FS_READ_ROOT *r)
{
    FS_CREATE_INTENT i;struct stat st,target,stage;int fd=-1,dir=-1;
    char parent[FS_INTENT_MAX_PATH],*slash,*leaf;int present=0;
    FS_READ_STATUS s=pending_intent(r,&present);
    if(s!=FS_READ_OK||!present)return s;
    fd=openat(r->fd,FS_INTENT_NAME,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return FS_READ_DENIED;
    if(fstat(fd,&st)<0||!S_ISREG(st.st_mode)||(st.st_nlink<1||st.st_nlink>2)||
       (st.st_mode&077)!=0||st.st_size!=(off_t)sizeof(i)||read(fd,&i,sizeof(i))!=(ssize_t)sizeof(i)||
       !intent_valid(&i)){close(fd);return FS_READ_DENIED;}
    close(fd);
    {char record_tmp[48];struct stat temp_record;
     snprintf(record_tmp,sizeof(record_tmp),".fstxn-%.32s",i.stage+5);
     if(fstatat(r->fd,record_tmp,&temp_record,AT_SYMLINK_NOFOLLOW)==0){
         if(!S_ISREG(temp_record.st_mode)||temp_record.st_ino!=st.st_ino||
            temp_record.st_dev!=st.st_dev||st.st_nlink!=2)return FS_READ_DENIED;
     }else if(errno!=ENOENT||st.st_nlink!=1)return FS_READ_DENIED;
    }
    /* Avoid any write until both the stage and target identities are checked. */
    if(fstatat(r->fd,i.stage,&stage,AT_SYMLINK_NOFOLLOW)<0){
        if(errno!=ENOENT)return FS_READ_DENIED;
        memset(&stage,0,sizeof(stage));
    }else if(!S_ISREG(stage.st_mode)||stage.st_dev!=(dev_t)i.dev||
             stage.st_ino!=(ino_t)i.ino||stage.st_nlink>2)return FS_READ_DENIED;
    strcpy(parent,i.target);slash=strrchr(parent,'/');
    if(slash){*slash=0;leaf=slash+1;}else{*parent=0;leaf=i.target;}
    s=posix_open(r,parent,&dir);if(s!=FS_READ_OK)return s;
    if(fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)==0){
        if(!stage.st_ino||!S_ISREG(target.st_mode)||
           target.st_dev!=(dev_t)i.dev||target.st_ino!=(ino_t)i.ino){
            s=FS_READ_DENIED;goto done;
        }
        if(fsync(dir)<0){s=FS_READ_IO;goto done;}
    }else if(errno!=ENOENT){s=FS_READ_DENIED;goto done;}
    else if(!stage.st_ino){s=FS_READ_DENIED;goto done;}
    {char record_tmp[48];struct stat tmp;
     snprintf(record_tmp,sizeof(record_tmp),".fstxn-%.32s",i.stage+5);
     if(fstatat(r->fd,record_tmp,&tmp,AT_SYMLINK_NOFOLLOW)==0){
         if(!durable_remove(r->fd,record_tmp)){s=FS_READ_IO;goto done;}
     }else if(errno!=ENOENT){s=FS_READ_DENIED;goto done;}
    }
    /* Recheck the published target before removing the last recovery marker.
       Cooperative writers cannot change it while the lock is held. */
    if(fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)==0){
        if(!S_ISREG(target.st_mode)||target.st_dev!=(dev_t)i.dev||
           target.st_ino!=(ino_t)i.ino){s=FS_READ_DENIED;goto done;}
    }else if(errno!=ENOENT||!stage.st_ino){s=FS_READ_DENIED;goto done;}
    if(!durable_remove(r->fd,FS_INTENT_NAME)){s=FS_READ_IO;goto done;}
    /* The last marker is gone. Removing a stage before it would leave a
       target-absent replay with neither target nor stage, indistinguishable
       from interference. A crash here may leave an orphan stage instead. */
    if(stage.st_ino && !durable_remove(r->fd,i.stage)){s=FS_READ_IO;goto done;}
    s=FS_READ_OK;
done:close(dir);return s;
}
FS_READ_STATUS FsCreateRecover(const FS_READ_ROOT *r)
{
    int lockfd=-1;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;
    s=lock_workspace(r,&lockfd);if(s!=FS_READ_OK)return s;
    {
     {int batch=0,commit=0;
     s=batch_state(r,&batch,&commit);
     if(s!=FS_READ_OK||batch||commit){unlock_workspace(lockfd);
         return s==FS_READ_OK?FS_READ_DENIED:s;}
    }
     {int removal=0,committed=0;
      s=remove_state(r,&removal,&committed);
      if(s!=FS_READ_OK||removal||committed){unlock_workspace(lockfd);
          return s==FS_READ_OK?FS_READ_DENIED:s;}
     }
    }
    {int mv=0,commit=0;
     s=move_state(r,&mv,&commit);
     if(s!=FS_READ_OK||mv||commit){unlock_workspace(lockfd);
         return s==FS_READ_OK?FS_READ_DENIED:s;}
    }
    {int repl=0,commit=0;
     s=replace_state(r,&repl,&commit);
     if(s!=FS_READ_OK||repl||commit){unlock_workspace(lockfd);
         return s==FS_READ_OK?FS_READ_DENIED:s;}
    }
    s=recover_locked(r);unlock_workspace(lockfd);return s;
}
static int write_all(int fd,const unsigned char *bytes,size_t len)
{
    size_t used=0;
    while(used<len){ssize_t n=write(fd,bytes+used,len-used);
        if(n<=0)return 0;
        used+=(size_t)n;}
    return 1;
}
static FS_READ_STATUS create_locked(const FS_READ_ROOT *r,const char *rel,
                            const void *bytes,size_t len,unsigned mode)
{
    char parent[FS_INTENT_MAX_PATH],*slash,*leaf,temp_name[48]={0};
    char intent_tmp[48]={0};int dir=-1,temp=-1,record=-1,random_fd=-1,record_owned=0;
    unsigned char nonce[16];FS_READ_STATUS s=FS_READ_IO;int published=0,pending=0,own_intent=0;
    struct stat st;FS_CREATE_INTENT intent={0};
    FS_MANIFEST plan={0};FS_OP_REQUEST request={FS_OP_CREATE,NULL,rel};
    if(!r||!valid_relative(rel,0)||strlen(rel)>=sizeof(parent)||
       (!bytes&&len)||len>FS_READ_MAX||mode>0777)return FS_READ_INVALID;
    if(!strcmp(rel,".fstxn.lock")||!strcmp(rel,FS_INTENT_NAME)||
       !strcmp(rel,FS_BATCH_NAME)||!strcmp(rel,FS_BATCH_COMMIT)||
       !strcmp(rel,FS_REMOVE_NAME)||!strcmp(rel,FS_REMOVE_COMMIT)||
       !strcmp(rel,FS_MOVE_NAME)||!strcmp(rel,FS_MOVE_COMMIT)||
       !strcmp(rel,FS_REPLACE_NAME)||!strcmp(rel,FS_REPLACE_COMMIT)||
       !strncmp(rel,".fstxn/",7)||!strcmp(rel,".fstxn")||
       !strncmp(rel,".fst-",5)||!strncmp(rel,".fstxn-",7)||
       !strncmp(rel,".fsrm-",6)||!strncmp(rel,".fsmv-",6)||
       !strncmp(rel,".fsrp-",6)||!strncmp(rel,".fsrb-",6))return FS_READ_DENIED;
    s=pending_intent(r,&pending);if(s!=FS_READ_OK)goto done;
    if(pending){s=FS_READ_DENIED;goto done;}
    {int batch=0,commit=0;
     s=batch_state(r,&batch,&commit);
     if(s!=FS_READ_OK||batch||commit){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    {int removal=0,committed=0;
     s=remove_state(r,&removal,&committed);
     if(s!=FS_READ_OK||removal||committed){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    {int mv=0,committed=0;
     s=move_state(r,&mv,&committed);
     if(s!=FS_READ_OK||mv||committed){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    {int repl=0,commit=0;
     s=replace_state(r,&repl,&commit);
     if(s!=FS_READ_OK||repl||commit){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    s=FsManifestPlan(r,&request,1,&plan);if(s!=FS_READ_OK)goto done;
    FsManifestFree(&plan);
    strcpy(parent,rel);slash=strrchr(parent,'/');
    if(slash){*slash=0;leaf=slash+1;}else{*parent=0;leaf=(char*)rel;}
    s=posix_open(r,parent,&dir);if(s!=FS_READ_OK)goto done;
    {FS_READ_META m;s=posix_meta(dir,&m);
     if(s!=FS_READ_OK||m.kind!=FS_KIND_DIR){s=FS_READ_UNSUPPORTED;goto done;}}
    random_fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random_fd<0||read(random_fd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
        s=FS_READ_IO;goto done;
    }
    close(random_fd);random_fd=-1;
    memcpy(temp_name,".fst-",5);
    for(size_t k=0;k<sizeof(nonce);k++)sprintf(temp_name+5+k*2,"%02x",nonce[k]);
    temp_name[37]=0;
    temp=openat(r->fd,temp_name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(temp<0){s=error_status();goto done;}
    if(!write_all(temp,(const unsigned char*)bytes,len)||
       fchmod(temp,(mode_t)mode)<0||fsync(temp)<0||fstat(temp,&st)<0||
       fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    crash_point(1); /* durable stage, no intent */
    intent.magic=FS_INTENT_MAGIC;intent.version=1;
    intent.dev=(uint64_t)st.st_dev;intent.ino=(uint64_t)st.st_ino;
    strcpy(intent.target,rel);strcpy(intent.stage,temp_name);
    /* Distinct temporary record; the pending name is published only after
       the complete record has been synced. */
    snprintf(intent_tmp,sizeof(intent_tmp),".fstxn-%.32s",temp_name+5);
    record=openat(r->fd,intent_tmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(record<0){s=error_status();goto done;}
    record_owned=1;
    if(!write_all(record,(const unsigned char*)&intent,sizeof(intent))||
       fsync(record)<0){s=FS_READ_IO;goto done;}
    close(record);record=-1;
    if(linkat(r->fd,intent_tmp,r->fd,FS_INTENT_NAME,0)<0){
        s=error_status();goto done;
    }
    pending=1;own_intent=1;
    if(fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    crash_point(2); /* durable intent, no target */
    if(linkat(r->fd,temp_name,dir,leaf,0)<0){
        s=(errno==EEXIST)?FS_READ_DENIED:error_status();goto done;
    }
    published=1;
    crash_point(3); /* target visible but parent not synced */
    if(fsync(dir)<0){s=FS_READ_IO;goto done;}
    crash_point(4); /* durable target, pending intent */
    /* Target is visible. Cleanup failure is not a failed create: future
       creates refuse the pending intent until explicit recovery. */
    (void)recover_locked(r);
    s=FS_READ_OK;
done:
    if(record>=0)close(record);
    if(record_owned){(void)unlinkat(r->fd,intent_tmp,0);(void)fsync(r->fd);}
    if(temp>=0)close(temp);
    if(!pending&&temp_name[0]){(void)unlinkat(r->fd,temp_name,0);(void)fsync(r->fd);}
    if(own_intent&&!published){
        /* In-process failure after our intent and before the target name was
           linked: we know nothing was published, so undo exactly what this call
           wrote instead of leaving a pending intent that blocks the next
           writer. Only our own record (byte-identical) and our own stage are
           removed; anything else stays and the call reports IO. */
        FS_CREATE_INTENT cur;int rfd=openat(r->fd,FS_INTENT_NAME,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        int ok=0;
        if(rfd>=0){
            struct stat rs;
            ok=fstat(rfd,&rs)==0&&S_ISREG(rs.st_mode)&&rs.st_size==(off_t)sizeof(cur)&&
               read(rfd,&cur,sizeof(cur))==(ssize_t)sizeof(cur)&&!memcmp(&cur,&intent,sizeof(cur));
            close(rfd);
        }
        if(ok){
            struct stat ss;
            ok=durable_remove(r->fd,FS_INTENT_NAME);
            if(ok&&fstatat(r->fd,temp_name,&ss,AT_SYMLINK_NOFOLLOW)==0){
                if(S_ISREG(ss.st_mode)&&ss.st_dev==(dev_t)intent.dev&&ss.st_ino==(ino_t)intent.ino)
                    ok=durable_remove(r->fd,temp_name);
                else ok=0;
            }
        }
        if(!ok)s=FS_READ_IO;
    }
    if(dir>=0)close(dir);
    if(random_fd>=0)close(random_fd);
    /* A successful link must never be reported as failure and invite retry. */
    return published?FS_READ_OK:s;
}
#define FS_BATCH_LIMIT 8
#define FS_BATCH_MAGIC 0x46534241u
/* Format is same-host only; fixed-size records never accept partial reads. */
typedef struct {
    char target[FS_INTENT_MAX_PATH];
    char stage[48];
    uint64_t dev,ino;
} FS_BATCH_ITEM;
typedef struct {
    uint32_t magic,version,count,reserved;
    FS_BATCH_ITEM items[FS_BATCH_LIMIT];
} FS_BATCH_RECORD;
static int batch_name_ok(const char *p)
{
    return p && valid_relative(p,0) && strlen(p)<FS_INTENT_MAX_PATH &&
      strcmp(p,".fstxn.lock") && strcmp(p,FS_INTENT_NAME) &&
      strcmp(p,FS_BATCH_NAME) && strcmp(p,FS_BATCH_COMMIT) &&
      strcmp(p,FS_REMOVE_NAME) && strcmp(p,FS_REMOVE_COMMIT) &&
      strcmp(p,FS_MOVE_NAME) && strcmp(p,FS_MOVE_COMMIT) &&
      strcmp(p,FS_REPLACE_NAME) && strcmp(p,FS_REPLACE_COMMIT) &&
      strcmp(p,".fstxn") && strncmp(p,".fstxn/",7) &&
      strncmp(p,".fst-",5) && strncmp(p,".fstxn-",7) &&
      strncmp(p,".fsrm-",6) && strncmp(p,".fsmv-",6) &&
      strncmp(p,".fsrp-",6) && strncmp(p,".fsrb-",6);
}
static int batch_valid(const FS_BATCH_RECORD *b)
{
    if(b->magic!=FS_BATCH_MAGIC||b->version!=1||b->count<2||
       b->count>FS_BATCH_LIMIT||b->reserved)return 0;
    for(unsigned k=0;k<b->count;k++){
        const FS_BATCH_ITEM *i=&b->items[k];
        if(!memchr(i->target,0,sizeof(i->target))||
           !memchr(i->stage,0,sizeof(i->stage))||
           !batch_name_ok(i->target)||strncmp(i->stage,".fst-",5)||
           strlen(i->stage)!=37||strspn(i->stage+5,"0123456789abcdef")!=32)return 0;
        for(unsigned j=0;j<k;j++)
            if(!strcmp(i->target,b->items[j].target)||
               !strcmp(i->stage,b->items[j].stage))return 0;
    }
    for(unsigned k=b->count;k<FS_BATCH_LIMIT;k++){
        const unsigned char *p=(const unsigned char*)&b->items[k];
        for(size_t j=0;j<sizeof(b->items[k]);j++)if(p[j])return 0;
    }
    return 1;
}
static FS_READ_STATUS batch_state(const FS_READ_ROOT *r,int *batch,int *commit)
{
    struct stat st;
    *batch=*commit=0;
    if(fstatat(r->fd,FS_BATCH_NAME,&st,AT_SYMLINK_NOFOLLOW)==0)*batch=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    if(fstatat(r->fd,FS_BATCH_COMMIT,&st,AT_SYMLINK_NOFOLLOW)==0)*commit=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    return FS_READ_OK;
}
/* Multi-file replace journal (record version 2 under FS_BATCH_NAME). Same
   journal and commit names as the create batch, so every other writer already
   refuses to run while one is pending. Recovery is dispatched by record size. */
#define FS_BREPL_MAGIC 0x46534252u
typedef struct {
    char target[FS_INTENT_MAX_PATH];
    char oldstage[48],newstage[48];
    uint64_t olddev,oldino,newdev,newino;
} FS_BREPL_ITEM;
typedef struct {
    uint32_t magic,version,count,reserved;
    FS_BREPL_ITEM items[FS_BATCH_LIMIT];
} FS_BREPL_RECORD;
typedef char fs_brepl_size_differs[sizeof(FS_BREPL_RECORD)!=sizeof(FS_BATCH_RECORD)?1:-1];
static int brepl_stage_ok(const char *s)
{
    return strlen(s)==38 && !strncmp(s,".fsrp-",6) &&
           strspn(s+6,"0123456789abcdef")==32;
}
static int brepl_valid(const FS_BREPL_RECORD *b)
{
    if(b->magic!=FS_BREPL_MAGIC||b->version!=1||b->count<2||
       b->count>FS_BATCH_LIMIT||b->reserved)return 0;
    for(unsigned k=0;k<b->count;k++){
        const FS_BREPL_ITEM *i=&b->items[k];
        if(!memchr(i->target,0,sizeof(i->target))||
           !memchr(i->oldstage,0,sizeof(i->oldstage))||
           !memchr(i->newstage,0,sizeof(i->newstage))||
           !batch_name_ok(i->target)||!brepl_stage_ok(i->oldstage)||
           !brepl_stage_ok(i->newstage)||!strcmp(i->oldstage,i->newstage)||
           (i->olddev==i->newdev&&i->oldino==i->newino))return 0;
        for(unsigned j=0;j<k;j++){
            const FS_BREPL_ITEM *o=&b->items[j];
            if(!strcmp(i->target,o->target)||!strcmp(i->oldstage,o->oldstage)||
               !strcmp(i->oldstage,o->newstage)||!strcmp(i->newstage,o->oldstage)||
               !strcmp(i->newstage,o->newstage))return 0;
        }
    }
    for(unsigned k=b->count;k<FS_BATCH_LIMIT;k++){
        const unsigned char *p=(const unsigned char*)&b->items[k];
        for(size_t j=0;j<sizeof(b->items[k]);j++)if(p[j])return 0;
    }
    return 1;
}
static void brepl_rb_name(const FS_BREPL_ITEM *i,char *out)
{ snprintf(out,48,".fsrb-%.32s",i->newstage+6); }
static int brepl_is(const struct stat *st,uint64_t dev,uint64_t ino)
{ return S_ISREG(st->st_mode)&&st->st_dev==(dev_t)dev&&st->st_ino==(ino_t)ino; }
/* Caller holds the workspace lock. Without a commit marker every target that
   holds the new inode is restored from its pinned old inode; with a marker
   every target must already hold the new inode. Nothing is changed until every
   target, stage and rollback name has been validated. */
static FS_READ_STATUS brepl_recover_locked(const FS_READ_ROOT *r,int commit)
{
    FS_BREPL_RECORD b;struct stat journal,mark,t;
    int dirs[FS_BATCH_LIMIT],tnew[FS_BATCH_LIMIT],oldp[FS_BATCH_LIMIT];
    int newp[FS_BATCH_LIMIT],rbp[FS_BATCH_LIMIT];
    char rb[FS_BATCH_LIMIT][48],parent[FS_INTENT_MAX_PATH],*slash;
    const char *leaf;int fd;FS_READ_STATUS s=FS_READ_DENIED;
    for(unsigned k=0;k<FS_BATCH_LIMIT;k++)dirs[k]=-1;
    fd=openat(r->fd,FS_BATCH_NAME,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return FS_READ_DENIED;
    if(fstat(fd,&journal)<0||!S_ISREG(journal.st_mode)||
       (journal.st_mode&077)!=0||journal.st_nlink!=1||
       journal.st_size!=(off_t)sizeof(b)||
       read(fd,&b,sizeof(b))!=(ssize_t)sizeof(b)||!brepl_valid(&b)){
        close(fd);return FS_READ_DENIED;
    }
    close(fd);
    if(commit){
        uint64_t identity=0;
        int mk=openat(r->fd,FS_BATCH_COMMIT,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        if(mk<0)return FS_READ_DENIED;
        if(fstat(mk,&mark)<0||!S_ISREG(mark.st_mode)||mark.st_nlink!=1||
           mark.st_size!=(off_t)sizeof(identity)||
           read(mk,&identity,sizeof(identity))!=(ssize_t)sizeof(identity)||
           identity!=(uint64_t)journal.st_ino){close(mk);return FS_READ_DENIED;}
        close(mk);
    }
    for(unsigned k=0;k<b.count;k++){
        FS_BREPL_ITEM *i=&b.items[k];
        strcpy(parent,i->target);slash=strrchr(parent,'/');
        if(slash)*slash=0;else *parent=0;
        leaf=slash?slash+1:i->target;
        s=posix_open(r,parent,&dirs[k]);if(s!=FS_READ_OK)goto done;
        s=FS_READ_DENIED;
        brepl_rb_name(i,rb[k]);
        oldp[k]=fstatat(r->fd,i->oldstage,&t,AT_SYMLINK_NOFOLLOW)==0;
        if(oldp[k]?!brepl_is(&t,i->olddev,i->oldino):errno!=ENOENT)goto done;
        newp[k]=fstatat(r->fd,i->newstage,&t,AT_SYMLINK_NOFOLLOW)==0;
        if(newp[k]?!brepl_is(&t,i->newdev,i->newino):errno!=ENOENT)goto done;
        rbp[k]=fstatat(dirs[k],rb[k],&t,AT_SYMLINK_NOFOLLOW)==0;
        if(rbp[k]?!brepl_is(&t,i->olddev,i->oldino):errno!=ENOENT)goto done;
        if(fstatat(dirs[k],leaf,&t,AT_SYMLINK_NOFOLLOW)<0||!S_ISREG(t.st_mode))goto done;
        if(brepl_is(&t,i->newdev,i->newino))tnew[k]=1;
        else if(brepl_is(&t,i->olddev,i->oldino))tnew[k]=0;
        else goto done;
        if(commit&&!tnew[k])goto done;
        if(!commit&&tnew[k]&&!oldp[k]&&!rbp[k])goto done;
    }
    for(unsigned k=0;k<b.count;k++){
        FS_BREPL_ITEM *i=&b.items[k];
        strcpy(parent,i->target);slash=strrchr(parent,'/');
        leaf=slash?slash+1:i->target;
        if(!commit&&tnew[k]){
            if(!rbp[k]){
                if(linkat(r->fd,i->oldstage,dirs[k],rb[k],0)<0){s=FS_READ_IO;goto done;}
                crash_point(65); /* rollback link exists, target still new */
            }
            if(renameat(dirs[k],rb[k],dirs[k],leaf)<0){s=FS_READ_IO;goto done;}
        }else if(rbp[k]&&unlinkat(dirs[k],rb[k],0)<0){s=FS_READ_IO;goto done;}
        if(fsync(dirs[k])<0){s=FS_READ_IO;goto done;}
    }
    if(commit){
        if(!durable_remove(r->fd,FS_BATCH_NAME)){s=FS_READ_IO;goto done;}
        crash_point(66); /* journal retired, marker remains */
        if(!durable_remove(r->fd,FS_BATCH_COMMIT)){s=FS_READ_IO;goto done;}
    }else if(!durable_remove(r->fd,FS_BATCH_NAME)){s=FS_READ_IO;goto done;}
    for(unsigned k=0;k<b.count;k++){
        if(oldp[k])(void)durable_remove(r->fd,b.items[k].oldstage);
        if(newp[k])(void)durable_remove(r->fd,b.items[k].newstage);
    }
    s=FS_READ_OK;
done:
    for(unsigned k=0;k<FS_BATCH_LIMIT;k++)if(dirs[k]>=0)close(dirs[k]);
    return s;
}
static FS_READ_STATUS batch_recover_locked(const FS_READ_ROOT *r)
{
    FS_BATCH_RECORD b;struct stat journal,mark,stage[FS_BATCH_LIMIT],target[FS_BATCH_LIMIT];
    int dirs[FS_BATCH_LIMIT],present[FS_BATCH_LIMIT],stages[FS_BATCH_LIMIT];
    char parent[FS_INTENT_MAX_PATH],*slash,*leaf;
    int fd=-1,exists=0,commit=0;FS_READ_STATUS s=batch_state(r,&exists,&commit);
    if(s!=FS_READ_OK)return s;
    if(!exists){
        /* The target list is gone. A marker-only crash remains fail-closed;
           the caller must inspect it manually, never infer safe cleanup. */
        return commit?FS_READ_DENIED:FS_READ_OK;
    }
    {struct stat js;
     if(fstatat(r->fd,FS_BATCH_NAME,&js,AT_SYMLINK_NOFOLLOW)==0&&
        js.st_size==(off_t)sizeof(FS_BREPL_RECORD))
         return brepl_recover_locked(r,commit);}
    for(unsigned k=0;k<FS_BATCH_LIMIT;k++)dirs[k]=-1;
    fd=openat(r->fd,FS_BATCH_NAME,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return FS_READ_DENIED;
    if(fstat(fd,&journal)<0||!S_ISREG(journal.st_mode)||
       (journal.st_mode&077)!=0||journal.st_nlink!=1||
       journal.st_size!=(off_t)sizeof(b)||read(fd,&b,sizeof(b))!=(ssize_t)sizeof(b)||
       !batch_valid(&b)){close(fd);return FS_READ_DENIED;}
    close(fd);
    if(commit){int markfd=openat(r->fd,FS_BATCH_COMMIT,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        uint64_t identity=0;
        if(markfd<0)return FS_READ_DENIED;
        if(fstat(markfd,&mark)<0||!S_ISREG(mark.st_mode)||mark.st_nlink!=1||
           mark.st_size!=(off_t)sizeof(identity)||
           read(markfd,&identity,sizeof(identity))!=(ssize_t)sizeof(identity)||
           identity!=(uint64_t)journal.st_ino){close(markfd);return FS_READ_DENIED;}
        close(markfd);
    }
    /* Validate every target and stage before any unlink; no partial rollback
       on an identity conflict discovered in a later entry. */
    for(unsigned k=0;k<b.count;k++){
        FS_BATCH_ITEM *i=&b.items[k];
        stages[k]=fstatat(r->fd,i->stage,&stage[k],AT_SYMLINK_NOFOLLOW)==0;
        if(!stages[k]&&errno!=ENOENT){s=FS_READ_DENIED;goto done;}
        if(stages[k]&&(!S_ISREG(stage[k].st_mode)||
             stage[k].st_dev!=(dev_t)i->dev||stage[k].st_ino!=(ino_t)i->ino||
             stage[k].st_nlink>2)){s=FS_READ_DENIED;goto done;}
        strcpy(parent,i->target);slash=strrchr(parent,'/');
        if(slash)*slash=0;else *parent=0;
        s=posix_open(r,parent,&dirs[k]);if(s!=FS_READ_OK)goto done;
        leaf=slash?slash+1:i->target;
        present[k]=fstatat(dirs[k],leaf,&target[k],AT_SYMLINK_NOFOLLOW)==0;
        if(!present[k]&&errno!=ENOENT){s=FS_READ_DENIED;goto done;}
        if(present[k]&&(!S_ISREG(target[k].st_mode)||
            target[k].st_dev!=(dev_t)i->dev||target[k].st_ino!=(ino_t)i->ino||
            !stages[k]||target[k].st_nlink>2)){s=FS_READ_DENIED;goto done;}
        if(!present[k]&&!stages[k]){s=FS_READ_DENIED;goto done;}
        if(commit&&!present[k]){s=FS_READ_DENIED;goto done;}
    }
    for(unsigned k=0;k<b.count;k++){
        FS_BATCH_ITEM *i=&b.items[k];
        strcpy(parent,i->target);slash=strrchr(parent,'/');leaf=slash?slash+1:i->target;
        /* Commit preserves every target. No marker means roll back matching
           published targets, then sync each parent before retiring journal. */
        if(!commit&&present[k]){
            if(unlinkat(dirs[k],leaf,0)<0||fsync(dirs[k])<0){s=FS_READ_IO;goto done;}
        }else if(commit&&fsync(dirs[k])<0){s=FS_READ_IO;goto done;}
    }
    /* Keep staged files until the journal is retired. Crashes during cleanup
       leave orphans, rather than making an interrupted replay ambiguous. */
    if(commit){
        /* A commit marker is never removed before the journal. If power is
           lost between removals, marker-only state remains fail-closed. */
        if(!durable_remove(r->fd,FS_BATCH_NAME)){s=FS_READ_IO;goto done;}
        crash_point(8); /* committed target list retired, marker remains */
        if(!durable_remove(r->fd,FS_BATCH_COMMIT)){s=FS_READ_IO;goto done;}
    }else if(!durable_remove(r->fd,FS_BATCH_NAME)){s=FS_READ_IO;goto done;}
    for(unsigned k=0;k<b.count;k++)if(stages[k])
        (void)durable_remove(r->fd,b.items[k].stage);
    s=FS_READ_OK;
done:
    for(unsigned k=0;k<FS_BATCH_LIMIT;k++)if(dirs[k]>=0)close(dirs[k]);
    return s;
}
FS_READ_STATUS FsBatchRecover(const FS_READ_ROOT *r)
{
    int lock=-1,pending=0;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);
    if(s==FS_READ_OK){int removal=0,committed=0;
      s=remove_state(r,&removal,&committed);
      if(s==FS_READ_OK){int mv=0,mc=0;
      s=move_state(r,&mv,&mc);
      if(s==FS_READ_OK){int repl=0,pc=0;
       s=replace_state(r,&repl,&pc);
       if(s==FS_READ_OK)s=(pending||removal||committed||mv||mc||repl||pc)?
           FS_READ_DENIED:batch_recover_locked(r);
      }
    }
    }
    unlock_workspace(lock);return s;
}
FS_READ_STATUS FsBatchCreate(const FS_READ_ROOT *r,const FS_BATCH_CREATE *entries,size_t count)
{
    FS_BATCH_RECORD b={0};FS_OP_REQUEST req[FS_BATCH_LIMIT];FS_MANIFEST manifest={0};
    unsigned char nonce[16];int lock=-1,random_fd=-1,record=-1;
    int dirs[FS_BATCH_LIMIT],stagefd[FS_BATCH_LIMIT];
    char parent[FS_INTENT_MAX_PATH],*slash,*leaf;int pending=0,other=0,commit=0;
    int journaled=0,visible=0,committed_here=0;FS_READ_STATUS s=FS_READ_IO;
    struct stat st;
    if(!r||!entries||count<2||count>FS_BATCH_LIMIT)return FS_READ_INVALID;
    for(size_t k=0;k<count;k++){
        if(!batch_name_ok(entries[k].target)||
           (!entries[k].bytes&&entries[k].len)||entries[k].len>FS_READ_MAX||
           entries[k].mode>0777)return FS_READ_INVALID;
        req[k]=(FS_OP_REQUEST){FS_OP_CREATE,NULL,entries[k].target};
    }
    for(unsigned k=0;k<FS_BATCH_LIMIT;k++)dirs[k]=stagefd[k]=-1;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&other,&commit);if(s!=FS_READ_OK)goto done;
    if(pending||other||commit){s=FS_READ_DENIED;goto done;}
    {int removal=0,committed=0;
     s=remove_state(r,&removal,&committed);
     if(s!=FS_READ_OK||removal||committed){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    {int mv=0,mc=0;
     s=move_state(r,&mv,&mc);
     if(s!=FS_READ_OK||mv||mc){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    {int repl=0,pc=0;
     s=replace_state(r,&repl,&pc);
     if(s!=FS_READ_OK||repl||pc){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    s=FsManifestPlan(r,req,count,&manifest);if(s!=FS_READ_OK)goto done;
    FsManifestFree(&manifest);
    b.magic=FS_BATCH_MAGIC;b.version=1;b.count=(uint32_t)count;
    random_fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random_fd<0){s=FS_READ_IO;goto done;}
    for(size_t k=0;k<count;k++){
        FS_BATCH_ITEM *i=&b.items[k];
        struct stat file;
        strcpy(i->target,entries[k].target);
        strcpy(parent,i->target);slash=strrchr(parent,'/');
        if(slash)*slash=0;else *parent=0;
        s=posix_open(r,parent,&dirs[k]);if(s!=FS_READ_OK)goto done;
        if(read(random_fd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
            s=FS_READ_IO;goto done;}
        memcpy(i->stage,".fst-",5);
        for(size_t j=0;j<sizeof(nonce);j++)sprintf(i->stage+5+j*2,"%02x",nonce[j]);
        i->stage[37]=0;
        stagefd[k]=openat(r->fd,i->stage,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if(stagefd[k]<0){s=error_status();goto done;}
        if(!write_all(stagefd[k],entries[k].bytes,entries[k].len)||
           fchmod(stagefd[k],(mode_t)entries[k].mode)<0||
           fsync(stagefd[k])<0||fstat(stagefd[k],&file)<0){s=FS_READ_IO;goto done;}
        i->dev=(uint64_t)file.st_dev;i->ino=(uint64_t)file.st_ino;
    }
    if(fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    record=openat(r->fd,FS_BATCH_NAME,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(record<0){s=error_status();goto done;}
    journaled=1;
    if(!write_all(record,(const unsigned char*)&b,sizeof(b))||
       fsync(record)<0||fstat(record,&st)<0||fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    crash_point(5); /* durable journal; no target */
    for(size_t k=0;k<count;k++){
        strcpy(parent,b.items[k].target);slash=strrchr(parent,'/');
        leaf=slash?slash+1:b.items[k].target;
        if(linkat(r->fd,b.items[k].stage,dirs[k],leaf,0)<0){
            s=errno==EEXIST?FS_READ_DENIED:error_status();goto done;
        }
        visible=1;
        if(fsync(dirs[k])<0){s=FS_READ_IO;goto done;}
        if(k==0)crash_point(6); /* partial published batch */
    }
    /* A synced commit marker names the exact journal inode; no rollback
       after this point. If marking fails, recovery rolls the batch back. */
    {uint64_t id=(uint64_t)st.st_ino;
     int mark=openat(r->fd,FS_BATCH_COMMIT,
             O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
     if(mark<0){s=error_status();goto done;}
     if(!write_all(mark,(const unsigned char*)&id,sizeof(id))||fsync(mark)<0){
         close(mark);s=FS_READ_IO;goto done;
     }
     close(mark);
    }
    if(fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    committed_here=1;
    crash_point(7); /* committed, cleanup still pending */
    (void)batch_recover_locked(r);
    s=FS_READ_OK;
done:
    if(record>=0)close(record);
    for(size_t k=0;k<FS_BATCH_LIMIT;k++){
        if(stagefd[k]>=0)close(stagefd[k]);
        if(dirs[k]>=0)close(dirs[k]);
    }
    if(!journaled){
        for(size_t k=0;k<count;k++)if(b.items[k].stage[0])
            (void)unlinkat(r->fd,b.items[k].stage,0);
        (void)fsync(r->fd);
    }
    if(random_fd>=0)close(random_fd);
    if(journaled&&!committed_here){
        /* In-process failure after the journal and before the commit marker:
           roll back now with the crash-replay code (same contract as
           FsBatchReplace). Success keeps the original error; a failed
           rollback leaves the journal and reports IO. */
        int pj=0,pc=0;FS_READ_STATUS rs=batch_state(r,&pj,&pc);
        if(rs==FS_READ_OK&&pj&&!pc){
            if(batch_recover_locked(r)==FS_READ_OK)visible=0;
            else{s=FS_READ_IO;visible=1;}
        }else if(rs!=FS_READ_OK||pj){s=FS_READ_IO;visible=1;}
    }
    unlock_workspace(lock);
    /* If any name became visible, report an interrupted transaction as IO,
       never invite blind retry. Caller must recover explicitly. */
    return committed_here?FS_READ_OK:(visible?FS_READ_IO:s);
}
FS_READ_STATUS FsBatchReplace(const FS_READ_ROOT *r,const FS_BATCH_REPLACE *entries,size_t count)
{
    FS_BREPL_RECORD b={0};unsigned char nonce[16];struct stat st,old,nw;
    int lock=-1,random_fd=-1,record=-1,dirs[FS_BATCH_LIMIT];
    char parent[FS_INTENT_MAX_PATH],*slash;const char *leaf;
    int journaled=0,visible=0,committed_here=0,p=0,o=0,c=0;
    FS_READ_STATUS s=FS_READ_IO;
    if(!r||!entries||count<2||count>FS_BATCH_LIMIT)return FS_READ_INVALID;
    for(size_t k=0;k<count;k++){
        if(!entries[k].target||strlen(entries[k].target)>=FS_INTENT_MAX_PATH||
           !batch_name_ok(entries[k].target)||!entries[k].expected||
           (!entries[k].replacement&&entries[k].replacement_len)||
           entries[k].expected_len>FS_READ_MAX||
           entries[k].replacement_len>FS_READ_MAX)return FS_READ_INVALID;
        for(size_t j=0;j<k;j++)
            if(!strcmp(entries[k].target,entries[j].target))return FS_READ_INVALID;
    }
    for(unsigned k=0;k<FS_BATCH_LIMIT;k++)dirs[k]=-1;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&p);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&o,&c);if(s!=FS_READ_OK)goto done;
    if(p||o||c){s=FS_READ_DENIED;goto done;}
    {int a=0,bb=0;
     s=remove_state(r,&a,&bb);
     if(s!=FS_READ_OK||a||bb){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
     s=move_state(r,&a,&bb);
     if(s!=FS_READ_OK||a||bb){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
     s=replace_state(r,&a,&bb);
     if(s!=FS_READ_OK||a||bb){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    random_fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random_fd<0){s=FS_READ_IO;goto done;}
    b.magic=FS_BREPL_MAGIC;b.version=1;b.count=(uint32_t)count;
    /* Phase 1: verify every expected image, pin every old inode, stage every
       new image. Nothing is visible under a target name yet. */
    for(size_t k=0;k<count;k++){
        FS_BREPL_ITEM *i=&b.items[k];FS_READ_META meta;unsigned char *bytes=NULL;
        size_t len=0;int nf,of;
        s=FsReadFile(r,entries[k].target,&bytes,&len,&meta);if(s!=FS_READ_OK)goto done;
        if(len!=entries[k].expected_len||
           (len&&memcmp(bytes,entries[k].expected,len))){free(bytes);s=FS_READ_DENIED;goto done;}
        free(bytes);
        strcpy(i->target,entries[k].target);
        strcpy(parent,i->target);slash=strrchr(parent,'/');
        if(slash)*slash=0;else *parent=0;
        leaf=slash?slash+1:i->target;
        s=posix_open(r,parent,&dirs[k]);if(s!=FS_READ_OK)goto done;
        if(fstatat(dirs[k],leaf,&old,AT_SYMLINK_NOFOLLOW)<0||
           !S_ISREG(old.st_mode)||old.st_nlink!=1){s=FS_READ_DENIED;goto done;}
        for(int w=0;w<2;w++){
            char *dst=w?i->newstage:i->oldstage;
            if(read(random_fd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
                s=FS_READ_IO;goto done;}
            memcpy(dst,".fsrp-",6);
            for(size_t j=0;j<sizeof(nonce);j++)sprintf(dst+6+j*2,"%02x",nonce[j]);
            dst[38]=0;
        }
        if(linkat(dirs[k],leaf,r->fd,i->oldstage,0)<0){s=error_status();goto done;}
        of=openat(r->fd,i->oldstage,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(of<0){s=FS_READ_IO;goto done;}
        if(fstat(of,&st)<0||!brepl_is(&st,(uint64_t)old.st_dev,(uint64_t)old.st_ino)||
           fsync(of)<0){close(of);s=FS_READ_DENIED;goto done;}
        close(of);
        i->olddev=(uint64_t)old.st_dev;i->oldino=(uint64_t)old.st_ino;
        nf=openat(r->fd,i->newstage,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if(nf<0){s=error_status();goto done;}
        if(!write_all(nf,entries[k].replacement,entries[k].replacement_len)||
           fchmod(nf,old.st_mode&0777)<0||fsync(nf)<0||fstat(nf,&nw)<0){
            close(nf);s=FS_READ_IO;goto done;}
        close(nf);
        i->newdev=(uint64_t)nw.st_dev;i->newino=(uint64_t)nw.st_ino;
    }
    if(fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    record=openat(r->fd,FS_BATCH_NAME,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(record<0){s=error_status();goto done;}
    if(!write_all(record,(const unsigned char*)&b,sizeof(b))||
       fsync(record)<0||fstat(record,&st)<0||fsync(r->fd)<0){
        close(record);record=-1;
        if(!durable_remove(r->fd,FS_BATCH_NAME))journaled=1;
        s=FS_READ_IO;goto done;
    }
    journaled=1;
    crash_point(60); /* durable journal, every target still old */
    /* Phase 2: publish by rename onto the target name, one file at a time. */
    for(size_t k=0;k<count;k++){
        struct stat cur;
        strcpy(parent,b.items[k].target);slash=strrchr(parent,'/');
        leaf=slash?slash+1:b.items[k].target;
        if(fstatat(dirs[k],leaf,&cur,AT_SYMLINK_NOFOLLOW)<0||
           !brepl_is(&cur,b.items[k].olddev,b.items[k].oldino)){
            s=FS_READ_DENIED;goto done;}
        if(renameat(r->fd,b.items[k].newstage,dirs[k],leaf)<0){
            s=error_status();goto done;}
        visible=1;
        if(fsync(dirs[k])<0){s=FS_READ_IO;goto done;}
        if(k==0)crash_point(61); /* partial: first target new, rest old */
    }
    crash_point(62); /* every target new, no commit marker */
    {uint64_t id=(uint64_t)st.st_ino;
     int mark=openat(r->fd,FS_BATCH_COMMIT,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
     if(mark<0){s=error_status();goto done;}
     if(!write_all(mark,(const unsigned char*)&id,sizeof(id))||fsync(mark)<0){
         close(mark);s=FS_READ_IO;goto done;}
     close(mark);
    }
    if(fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    committed_here=1;
    crash_point(63); /* committed, cleanup pending */
    (void)brepl_recover_locked(r,1);
    s=FS_READ_OK;
done:
    if(record>=0)close(record);
    for(size_t k=0;k<FS_BATCH_LIMIT;k++)if(dirs[k]>=0)close(dirs[k]);
    if(random_fd>=0)close(random_fd);
    if(!journaled){
        for(size_t k=0;k<count;k++){
            if(b.items[k].oldstage[0])(void)unlinkat(r->fd,b.items[k].oldstage,0);
            if(b.items[k].newstage[0])(void)unlinkat(r->fd,b.items[k].newstage,0);
        }
        (void)fsync(r->fd);
    }else if(!committed_here){
        /* In-process failure after the journal: restore the original bytes
           now instead of leaving a pending transaction. */
        int pj=0,pc=0;FS_READ_STATUS rs=batch_state(r,&pj,&pc);
        if(rs==FS_READ_OK&&pj){
            rs=brepl_recover_locked(r,pc);
            if(rs==FS_READ_OK){if(pc){committed_here=1;s=FS_READ_OK;}}
            else {s=FS_READ_IO;visible=1;}
        }else if(rs!=FS_READ_OK){s=FS_READ_IO;visible=1;}
    }
    unlock_workspace(lock);
    return committed_here?FS_READ_OK:(visible&&s==FS_READ_IO?FS_READ_IO:s);
}
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *r,const char *rel,
                            const void *bytes,size_t len,unsigned mode)
{
    int lock=-1;FS_READ_STATUS s;
    if(!r||!valid_relative(rel,0)||(!bytes&&len)||len>FS_READ_MAX||mode>0777)
        return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=create_locked(r,rel,bytes,len,mode);
    unlock_workspace(lock);return s;
}
FS_READ_STATUS FsCopyFile(const FS_READ_ROOT *r,const char *src,const char *dst,
                          const void *expected,size_t expected_len)
{
    FS_OP_REQUEST request={FS_OP_COPY,src,dst};FS_MANIFEST plan={0};
    FS_READ_META meta;unsigned char *bytes=NULL;size_t len=0;
    FS_READ_STATUS s;int lock=-1;
    if(!r||!src||!dst||!expected||expected_len>FS_READ_MAX||
       !valid_relative(src,0)||!valid_relative(dst,0))return FS_READ_INVALID;
    if(!strcmp(dst,".fstxn.lock")||!strcmp(dst,FS_INTENT_NAME)||
       !strcmp(dst,FS_BATCH_NAME)||!strcmp(dst,FS_BATCH_COMMIT)||
       !strcmp(dst,FS_REMOVE_NAME)||!strcmp(dst,FS_REMOVE_COMMIT)||
       !strcmp(dst,FS_MOVE_NAME)||!strcmp(dst,FS_MOVE_COMMIT)||
       !strcmp(dst,FS_REPLACE_NAME)||!strcmp(dst,FS_REPLACE_COMMIT)||
       !strncmp(dst,".fstxn/",7)||!strcmp(dst,".fstxn")||
       !strncmp(dst,".fst-",5)||!strncmp(dst,".fstxn-",7)||
       !strncmp(dst,".fsrm-",6)||!strncmp(dst,".fsmv-",6)||
       !strncmp(dst,".fsrp-",6)||!strncmp(dst,".fsrb-",6))return FS_READ_DENIED;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=FsManifestPlan(r,&request,1,&plan);if(s!=FS_READ_OK)goto done;
    FsManifestFree(&plan);
    s=FsReadFile(r,src,&bytes,&len,&meta);if(s!=FS_READ_OK)goto done;
    if(len!=expected_len||(len&&memcmp(bytes,expected,len))){
        s=FS_READ_DENIED;goto done;
    }
    s=create_locked(r,dst,bytes,len,meta.mode);
done:
    free(bytes);FsManifestFree(&plan);unlock_workspace(lock);return s;
}
#define FS_REMOVE_MAGIC 0x4653524du
/* Same-host fixed record. The stage is a hard link to the original inode. */
typedef struct {
    uint32_t magic,version;
    uint64_t dev,ino;
    char target[FS_INTENT_MAX_PATH];
    char stage[48];
} FS_REMOVE_RECORD;
static FS_READ_STATUS remove_state(const FS_READ_ROOT *r,int *intent,int *commit)
{
    struct stat st;*intent=*commit=0;
    if(fstatat(r->fd,FS_REMOVE_NAME,&st,AT_SYMLINK_NOFOLLOW)==0)*intent=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    if(fstatat(r->fd,FS_REMOVE_COMMIT,&st,AT_SYMLINK_NOFOLLOW)==0)*commit=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    return FS_READ_OK;
}
static int remove_valid(const FS_REMOVE_RECORD *i)
{
    return i->magic==FS_REMOVE_MAGIC&&i->version==1&&
        memchr(i->target,0,sizeof(i->target))&&
        memchr(i->stage,0,sizeof(i->stage))&&
        batch_name_ok(i->target)&&
        strcmp(i->target,FS_REMOVE_NAME)&&strcmp(i->target,FS_REMOVE_COMMIT)&&
        strcmp(i->target,FS_MOVE_NAME)&&strcmp(i->target,FS_MOVE_COMMIT)&&
        strcmp(i->target,FS_REPLACE_NAME)&&strcmp(i->target,FS_REPLACE_COMMIT)&&
        strncmp(i->stage,".fsrm-",6)==0&&strlen(i->stage)==38&&
        strspn(i->stage+6,"0123456789abcdef")==32;
}
static FS_READ_STATUS remove_recover_locked(const FS_READ_ROOT *r)
{
    FS_REMOVE_RECORD i;struct stat rec,stage,target,marker;
    int present=0,commit=0,record=-1,dir=-1,stage_present,target_present;
    FS_READ_STATUS s=remove_state(r,&present,&commit);
    char parent[FS_INTENT_MAX_PATH],*slash,*leaf;
    if(s!=FS_READ_OK)return s;
    if(!present)return commit?FS_READ_DENIED:FS_READ_OK;
    record=openat(r->fd,FS_REMOVE_NAME,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(record<0)return FS_READ_DENIED;
    if(fstat(record,&rec)<0||!S_ISREG(rec.st_mode)||rec.st_nlink!=1||
       (rec.st_mode&077)!=0||rec.st_size!=(off_t)sizeof(i)||
       read(record,&i,sizeof(i))!=(ssize_t)sizeof(i)||!remove_valid(&i)){
        close(record);return FS_READ_DENIED;
    }
    close(record);
    if(commit){uint64_t id=0;int fd=openat(r->fd,FS_REMOVE_COMMIT,
            O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        if(fd<0)return FS_READ_DENIED;
        if(fstat(fd,&marker)<0||!S_ISREG(marker.st_mode)||
           marker.st_nlink!=1||marker.st_size!=(off_t)sizeof(id)||
           read(fd,&id,sizeof(id))!=(ssize_t)sizeof(id)||
           id!=(uint64_t)rec.st_ino){close(fd);return FS_READ_DENIED;}
        close(fd);
    }
    stage_present=fstatat(r->fd,i.stage,&stage,AT_SYMLINK_NOFOLLOW)==0;
    if(!stage_present)return FS_READ_DENIED;
    if(!S_ISREG(stage.st_mode)||stage.st_dev!=(dev_t)i.dev||
       stage.st_ino!=(ino_t)i.ino||stage.st_nlink>2)return FS_READ_DENIED;
    strcpy(parent,i.target);slash=strrchr(parent,'/');
    if(slash){*slash=0;leaf=slash+1;}else{*parent=0;leaf=i.target;}
    s=posix_open(r,parent,&dir);if(s!=FS_READ_OK)return s;
    target_present=fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)==0;
    if(!target_present&&errno!=ENOENT){s=FS_READ_DENIED;goto done;}
    if(target_present&&(!S_ISREG(target.st_mode)||
        target.st_dev!=(dev_t)i.dev||target.st_ino!=(ino_t)i.ino||
        target.st_nlink>2)){s=FS_READ_DENIED;goto done;}
    if(commit){
        if(target_present){s=FS_READ_DENIED;goto done;}
        /* Commit means removal stands. Retire record while marker remains;
           a crash here yields marker-only fail-closed state. */
    }else if(!target_present){
        if(linkat(r->fd,i.stage,dir,leaf,0)<0){s=FS_READ_DENIED;goto done;}
    }
    if(fsync(dir)<0){s=FS_READ_IO;goto done;}
    if(!durable_remove(r->fd,FS_REMOVE_NAME)){s=FS_READ_IO;goto done;}
    if(commit){crash_point(13);
        if(!durable_remove(r->fd,FS_REMOVE_COMMIT)){s=FS_READ_IO;goto done;}
    }
    (void)durable_remove(r->fd,i.stage);
    s=FS_READ_OK;
done:close(dir);return s;
}
FS_READ_STATUS FsRemoveRecover(const FS_READ_ROOT *r)
{
    int lock=-1,pending=0,batch=0,commit=0;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);
    if(s==FS_READ_OK)s=batch_state(r,&batch,&commit);
    if(s==FS_READ_OK){int mv=0,mc=0;
      s=move_state(r,&mv,&mc);
      if(s==FS_READ_OK){int repl=0,pc=0;
       s=replace_state(r,&repl,&pc);
       if(s==FS_READ_OK)s=(pending||batch||commit||mv||mc||repl||pc)?
           FS_READ_DENIED:remove_recover_locked(r);
      }
    }
    unlock_workspace(lock);return s;
}
FS_READ_STATUS FsRemoveFile(const FS_READ_ROOT *r,const char *path,
                            const void *expected,size_t expected_len)
{
    FS_REMOVE_RECORD i={0};FS_OP_REQUEST req={FS_OP_REMOVE,path,NULL};
    FS_MANIFEST plan={0};FS_READ_META m;unsigned char *bytes=NULL;
    size_t len=0;unsigned char nonce[16];struct stat source,record_stat,st;
    int lock=-1,source_fd=-1,dir=-1,random_fd=-1,record=-1,mark=-1;
    int pending=0,batch=0,committed=0,removal=0,rcommit=0;
    int stage_owned=0,journaled=0,unlinked=0,marked=0;
    char parent[FS_INTENT_MAX_PATH],*slash,*leaf;FS_READ_STATUS s=FS_READ_IO;
    if(!r||!path||!expected||expected_len>FS_READ_MAX||
       strlen(path)>=sizeof(parent)||!batch_name_ok(path)||
       !strcmp(path,FS_REMOVE_NAME)||!strcmp(path,FS_REMOVE_COMMIT)||
       !strncmp(path,".fsrm-",6)||!strcmp(path,FS_MOVE_NAME)||
       !strcmp(path,FS_MOVE_COMMIT)||!strncmp(path,".fsmv-",6)||
       !strcmp(path,FS_REPLACE_NAME)||!strcmp(path,FS_REPLACE_COMMIT)||
       !strncmp(path,".fsrp-",6)||!strncmp(path,".fsrb-",6))return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&batch,&committed);if(s!=FS_READ_OK)goto done;
    s=remove_state(r,&removal,&rcommit);if(s!=FS_READ_OK)goto done;
    if(pending||batch||committed||removal||rcommit){s=FS_READ_DENIED;goto done;}
    {int mv=0,mc=0;
     s=move_state(r,&mv,&mc);
     if(s!=FS_READ_OK||mv||mc){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    {int repl=0,pc=0;
     s=replace_state(r,&repl,&pc);
     if(s!=FS_READ_OK||repl||pc){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    s=FsManifestPlan(r,&req,1,&plan);if(s!=FS_READ_OK)goto done;
    FsManifestFree(&plan);
    s=FsReadFile(r,path,&bytes,&len,&m);if(s!=FS_READ_OK)goto done;
    if(len!=expected_len||(len&&memcmp(bytes,expected,len))){s=FS_READ_DENIED;goto done;}
    s=posix_open(r,path,&source_fd);if(s!=FS_READ_OK)goto done;
    if(fstat(source_fd,&source)<0||!S_ISREG(source.st_mode)||
       source.st_nlink!=1){s=FS_READ_DENIED;goto done;}
    strcpy(parent,path);slash=strrchr(parent,'/');
    if(slash){*slash=0;leaf=slash+1;}else{*parent=0;leaf=(char*)path;}
    s=posix_open(r,parent,&dir);if(s!=FS_READ_OK)goto done;
    if(fstatat(dir,leaf,&st,AT_SYMLINK_NOFOLLOW)<0||
       st.st_dev!=source.st_dev||st.st_ino!=source.st_ino||
       st.st_nlink!=1){s=FS_READ_DENIED;goto done;}
    random_fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random_fd<0||read(random_fd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
        s=FS_READ_IO;goto done;}
    i.magic=FS_REMOVE_MAGIC;i.version=1;
    i.dev=(uint64_t)source.st_dev;i.ino=(uint64_t)source.st_ino;
    strcpy(i.target,path);memcpy(i.stage,".fsrm-",6);
    for(size_t k=0;k<sizeof(nonce);k++)sprintf(i.stage+6+k*2,"%02x",nonce[k]);
    i.stage[38]=0;
    if(linkat(dir,leaf,r->fd,i.stage,0)<0){s=error_status();goto done;}
    stage_owned=1;
    if(fsync(source_fd)<0||fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    crash_point(10); /* stage exists; no journal */
    record=openat(r->fd,FS_REMOVE_NAME,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(record<0){s=error_status();goto done;}
    journaled=1;
    if(!write_all(record,(const unsigned char*)&i,sizeof(i))||
       fsync(record)<0||fstat(record,&record_stat)<0||fsync(r->fd)<0){
        s=FS_READ_IO;goto done;
    }
    crash_point(11); /* durable journal; target still present */
    if(unlinkat(dir,leaf,0)<0){s=error_status();goto done;}
    unlinked=1;
    if(fsync(dir)<0){s=FS_READ_IO;goto done;}
    crash_point(12); /* target absent; not committed */
    {uint64_t id=(uint64_t)record_stat.st_ino;
     mark=openat(r->fd,FS_REMOVE_COMMIT,
          O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
     if(mark<0){s=error_status();goto done;}
     if(!write_all(mark,(const unsigned char*)&id,sizeof(id))||fsync(mark)<0||
        fsync(r->fd)<0){s=FS_READ_IO;goto done;}
     marked=1;
    }
    crash_point(14); /* commit durable; target absent, cleanup pending */
    (void)remove_recover_locked(r);
    s=FS_READ_OK;
done:
    if(mark>=0)close(mark);
    if(record>=0)close(record);
    if(source_fd>=0)close(source_fd);
    if(dir>=0)close(dir);
    if(random_fd>=0)close(random_fd);
    free(bytes);FsManifestFree(&plan);
    if(!journaled&&stage_owned){(void)durable_remove(r->fd,i.stage);}
    if(journaled&&!unlinked&&!marked){
        /* Failure after the journal, target still present: roll back now
           (same contract as FsBatchReplace) instead of leaving it pending. */
        if(remove_recover_locked(r)!=FS_READ_OK)s=FS_READ_IO;
    }
    unlock_workspace(lock);
    return marked?FS_READ_OK:(unlinked?FS_READ_IO:s);
}
#define FS_MOVE_MAGIC 0x46534d56u
typedef struct {
    uint32_t magic,version;
    uint64_t dev,ino;
    char source[FS_INTENT_MAX_PATH],target[FS_INTENT_MAX_PATH];
    char stage[48];
} FS_MOVE_RECORD;
static FS_READ_STATUS move_state(const FS_READ_ROOT *r,int *intent,int *commit)
{
    struct stat st;*intent=*commit=0;
    if(fstatat(r->fd,FS_MOVE_NAME,&st,AT_SYMLINK_NOFOLLOW)==0)*intent=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    if(fstatat(r->fd,FS_MOVE_COMMIT,&st,AT_SYMLINK_NOFOLLOW)==0)*commit=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    return FS_READ_OK;
}
static int move_valid(const FS_MOVE_RECORD *i)
{
    if(i->magic!=FS_MOVE_MAGIC||i->version!=1||
       !memchr(i->source,0,sizeof(i->source))||
       !memchr(i->target,0,sizeof(i->target))||
       !memchr(i->stage,0,sizeof(i->stage))||
       !batch_name_ok(i->source)||!batch_name_ok(i->target)||
       !strcmp(i->source,i->target)||
       !strcmp(i->source,FS_MOVE_NAME)||!strcmp(i->source,FS_MOVE_COMMIT)||
       !strcmp(i->source,FS_REPLACE_NAME)||!strcmp(i->source,FS_REPLACE_COMMIT)||
       !strcmp(i->target,FS_MOVE_NAME)||!strcmp(i->target,FS_MOVE_COMMIT)||
       !strcmp(i->target,FS_REPLACE_NAME)||!strcmp(i->target,FS_REPLACE_COMMIT)||
       !strncmp(i->source,".fsrm-",6)||!strncmp(i->target,".fsrm-",6)||
       !strncmp(i->source,".fsmv-",6)||!strncmp(i->target,".fsmv-",6)||
       !strncmp(i->source,".fsrp-",6)||!strncmp(i->target,".fsrp-",6)||
       !strncmp(i->source,".fsrb-",6)||!strncmp(i->target,".fsrb-",6)||
       strncmp(i->stage,".fsmv-",6)||strlen(i->stage)!=38||
       strspn(i->stage+6,"0123456789abcdef")!=32)return 0;
    return 1;
}
static FS_READ_STATUS move_parent(const FS_READ_ROOT *r,const char *name,
                                   int *dir,const char **leaf)
{
    char parent[FS_INTENT_MAX_PATH],*slash;FS_READ_STATUS s;
    strcpy(parent,name);slash=strrchr(parent,'/');
    if(slash){*slash=0;*leaf=name+(slash-parent)+1;}
    else{*parent=0;*leaf=name;}
    s=posix_open(r,parent,dir);return s;
}
static FS_READ_STATUS move_recover_locked(const FS_READ_ROOT *r)
{
    FS_MOVE_RECORD i;struct stat rec,stage,source,target,marker;
    int present=0,commit=0,record=-1,source_dir=-1,target_dir=-1;
    int source_present,target_present;const char *source_leaf,*target_leaf;
    FS_READ_STATUS s=move_state(r,&present,&commit);
    if(s!=FS_READ_OK)return s;
    if(!present)return commit?FS_READ_DENIED:FS_READ_OK;
    record=openat(r->fd,FS_MOVE_NAME,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(record<0)return FS_READ_DENIED;
    if(fstat(record,&rec)<0||!S_ISREG(rec.st_mode)||rec.st_nlink!=1||
       (rec.st_mode&077)!=0||rec.st_size!=(off_t)sizeof(i)||
       read(record,&i,sizeof(i))!=(ssize_t)sizeof(i)||!move_valid(&i)){
        close(record);return FS_READ_DENIED;
    }
    close(record);
    if(commit){uint64_t id=0;int fd=openat(r->fd,FS_MOVE_COMMIT,
       O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
       if(fd<0)return FS_READ_DENIED;
       if(fstat(fd,&marker)<0||!S_ISREG(marker.st_mode)||marker.st_nlink!=1||
          marker.st_size!=(off_t)sizeof(id)||
          read(fd,&id,sizeof(id))!=(ssize_t)sizeof(id)||
          id!=(uint64_t)rec.st_ino){close(fd);return FS_READ_DENIED;}
       close(fd);
    }
    if(fstatat(r->fd,i.stage,&stage,AT_SYMLINK_NOFOLLOW)<0||
       !S_ISREG(stage.st_mode)||stage.st_dev!=(dev_t)i.dev||
       stage.st_ino!=(ino_t)i.ino||stage.st_nlink>3)return FS_READ_DENIED;
    s=move_parent(r,i.source,&source_dir,&source_leaf);if(s!=FS_READ_OK)return s;
    s=move_parent(r,i.target,&target_dir,&target_leaf);
    if(s!=FS_READ_OK){close(source_dir);return s;}
    source_present=fstatat(source_dir,source_leaf,&source,AT_SYMLINK_NOFOLLOW)==0;
    if(!source_present&&errno!=ENOENT){s=FS_READ_DENIED;goto done;}
    target_present=fstatat(target_dir,target_leaf,&target,AT_SYMLINK_NOFOLLOW)==0;
    if(!target_present&&errno!=ENOENT){s=FS_READ_DENIED;goto done;}
    if(source_present&&(!S_ISREG(source.st_mode)||source.st_dev!=(dev_t)i.dev||
       source.st_ino!=(ino_t)i.ino||source.st_nlink>3)){s=FS_READ_DENIED;goto done;}
    if(target_present&&(!S_ISREG(target.st_mode)||target.st_dev!=(dev_t)i.dev||
       target.st_ino!=(ino_t)i.ino||target.st_nlink>3)){s=FS_READ_DENIED;goto done;}
    if(commit){
        if(source_present||!target_present){s=FS_READ_DENIED;goto done;}
        if(fsync(target_dir)<0||fsync(source_dir)<0){s=FS_READ_IO;goto done;}
    }else{
        if(!source_present){
            if(linkat(r->fd,i.stage,source_dir,source_leaf,0)<0){s=FS_READ_DENIED;goto done;}
            if(fsync(source_dir)<0){s=FS_READ_IO;goto done;}
        }
        if(target_present){
            if(unlinkat(target_dir,target_leaf,0)<0||fsync(target_dir)<0){s=FS_READ_IO;goto done;}
        }
    }
    if(!durable_remove(r->fd,FS_MOVE_NAME)){s=FS_READ_IO;goto done;}
    if(commit){crash_point(24);
        if(!durable_remove(r->fd,FS_MOVE_COMMIT)){s=FS_READ_IO;goto done;}
    }
    (void)durable_remove(r->fd,i.stage);s=FS_READ_OK;
done:close(source_dir);close(target_dir);return s;
}
FS_READ_STATUS FsMoveRecover(const FS_READ_ROOT *r)
{
    int lock=-1,pending=0,batch=0,commit=0,remove=0,rc=0;FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);
    if(s==FS_READ_OK)s=batch_state(r,&batch,&commit);
    if(s==FS_READ_OK)s=remove_state(r,&remove,&rc);
    if(s==FS_READ_OK){int repl=0,pc=0;
     s=replace_state(r,&repl,&pc);
     if(s==FS_READ_OK)s=(pending||batch||commit||remove||rc||repl||pc)?
         FS_READ_DENIED:move_recover_locked(r);
    }
    unlock_workspace(lock);return s;
}
FS_READ_STATUS FsMoveFile(const FS_READ_ROOT *r,const char *src,const char *dst,
                          const void *expected,size_t expected_len)
{
    FS_MOVE_RECORD i={0};FS_OP_REQUEST req={FS_OP_MOVE,src,dst};
    FS_MANIFEST plan={0};FS_READ_META m;unsigned char *bytes=NULL;
    size_t len=0;unsigned char nonce[16];struct stat source,record_stat,st;
    int lock=-1,srcfd=-1,source_dir=-1,target_dir=-1,random_fd=-1;
    int record=-1,mark=-1,pending=0,batch=0,bc=0,remove=0,rc=0,mv=0,mc=0;
    int stage_owned=0,journaled=0,target_visible=0,source_removed=0,marked=0;
    const char *source_leaf,*target_leaf;FS_READ_STATUS s=FS_READ_IO;
    if(!r||!src||!dst||!expected||expected_len>FS_READ_MAX||
       strlen(src)>=FS_INTENT_MAX_PATH||strlen(dst)>=FS_INTENT_MAX_PATH||
       !batch_name_ok(src)||!batch_name_ok(dst)||!strcmp(src,dst)||
       !strcmp(src,FS_MOVE_NAME)||!strcmp(src,FS_MOVE_COMMIT)||
       !strcmp(dst,FS_MOVE_NAME)||!strcmp(dst,FS_MOVE_COMMIT)||
       !strcmp(dst,FS_REPLACE_NAME)||!strcmp(dst,FS_REPLACE_COMMIT)||
       !strncmp(src,".fsmv-",6)||!strncmp(dst,".fsmv-",6)||
       !strncmp(src,".fsrm-",6)||!strncmp(dst,".fsrm-",6)||
       !strcmp(src,FS_REPLACE_NAME)||!strcmp(dst,FS_REPLACE_NAME)||
       !strcmp(src,FS_REPLACE_COMMIT)||!strcmp(dst,FS_REPLACE_COMMIT)||
       !strncmp(src,".fsrp-",6)||!strncmp(dst,".fsrp-",6)||
       !strncmp(src,".fsrb-",6)||!strncmp(dst,".fsrb-",6))return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&batch,&bc);if(s!=FS_READ_OK)goto done;
    s=remove_state(r,&remove,&rc);if(s!=FS_READ_OK)goto done;
    s=move_state(r,&mv,&mc);if(s!=FS_READ_OK)goto done;
    if(pending||batch||bc||remove||rc||mv||mc){s=FS_READ_DENIED;goto done;}
    {int repl=0,pc=0;
     s=replace_state(r,&repl,&pc);
     if(s!=FS_READ_OK||repl||pc){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
    }
    s=FsManifestPlan(r,&req,1,&plan);if(s!=FS_READ_OK)goto done;
    FsManifestFree(&plan);
    s=FsReadFile(r,src,&bytes,&len,&m);if(s!=FS_READ_OK)goto done;
    if(len!=expected_len||(len&&memcmp(bytes,expected,len))){s=FS_READ_DENIED;goto done;}
    s=posix_open(r,src,&srcfd);if(s!=FS_READ_OK)goto done;
    if(fstat(srcfd,&source)<0||!S_ISREG(source.st_mode)||
       source.st_nlink!=1){s=FS_READ_DENIED;goto done;}
    s=move_parent(r,src,&source_dir,&source_leaf);if(s!=FS_READ_OK)goto done;
    s=move_parent(r,dst,&target_dir,&target_leaf);if(s!=FS_READ_OK)goto done;
    if(fstatat(source_dir,source_leaf,&st,AT_SYMLINK_NOFOLLOW)<0||
       st.st_dev!=source.st_dev||st.st_ino!=source.st_ino||st.st_nlink!=1){
       s=FS_READ_DENIED;goto done;
    }
    random_fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random_fd<0||read(random_fd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
       s=FS_READ_IO;goto done;}
    i.magic=FS_MOVE_MAGIC;i.version=1;
    i.dev=(uint64_t)source.st_dev;i.ino=(uint64_t)source.st_ino;
    strcpy(i.source,src);strcpy(i.target,dst);memcpy(i.stage,".fsmv-",6);
    for(size_t k=0;k<sizeof(nonce);k++)sprintf(i.stage+6+k*2,"%02x",nonce[k]);
    i.stage[38]=0;
    if(linkat(source_dir,source_leaf,r->fd,i.stage,0)<0){s=error_status();goto done;}
    stage_owned=1;
    if(fsync(srcfd)<0||fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    crash_point(20); /* stage before journal */
    record=openat(r->fd,FS_MOVE_NAME,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(record<0){s=error_status();goto done;}
    journaled=1;
    if(!write_all(record,(const unsigned char*)&i,sizeof(i))||
       fsync(record)<0||fstat(record,&record_stat)<0||fsync(r->fd)<0){
       s=FS_READ_IO;goto done;
    }
    crash_point(21); /* journal, source present, target absent */
    if(linkat(r->fd,i.stage,target_dir,target_leaf,0)<0){
       s=errno==EEXIST?FS_READ_DENIED:error_status();goto done;
    }
    target_visible=1;
    if(fsync(target_dir)<0){s=FS_READ_IO;goto done;}
    crash_point(22); /* target visible, source present */
    if(unlinkat(source_dir,source_leaf,0)<0){s=error_status();goto done;}
    source_removed=1;
    if(fsync(source_dir)<0){s=FS_READ_IO;goto done;}
    crash_point(23); /* target visible, source absent; no marker */
    {uint64_t id=(uint64_t)record_stat.st_ino;
     mark=openat(r->fd,FS_MOVE_COMMIT,
           O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
     if(mark<0){s=error_status();goto done;}
     if(!write_all(mark,(const unsigned char*)&id,sizeof(id))||
        fsync(mark)<0||fsync(r->fd)<0){s=FS_READ_IO;goto done;}
     marked=1;
    }
    crash_point(25); /* durable commit before cleanup */
    (void)move_recover_locked(r);s=FS_READ_OK;
done:
    if(mark>=0)close(mark);
    if(record>=0)close(record);
    if(srcfd>=0)close(srcfd);
    if(source_dir>=0)close(source_dir);
    if(target_dir>=0)close(target_dir);
    if(random_fd>=0)close(random_fd);
    free(bytes);FsManifestFree(&plan);
    if(!journaled&&stage_owned)(void)durable_remove(r->fd,i.stage);
    if(journaled&&!marked&&!source_removed){
        /* The source name is still intact. Roll back now, with the same code a
           crash replay uses: it removes the target link if this call made it. */
        if(move_recover_locked(r)==FS_READ_OK)target_visible=0;
        else s=FS_READ_IO;
    }
    unlock_workspace(lock);
    return marked?FS_READ_OK:((target_visible||source_removed)?FS_READ_IO:s);
}
#define FS_REPLACE_MAGIC 0x46535250u
#define FS_REPLACE_MARK_MAGIC 0x46535243u
typedef struct {
    uint32_t magic,version;
    uint64_t olddev,oldino,newdev,newino;
    char target[FS_INTENT_MAX_PATH],oldstage[48],newstage[48];
} FS_REPLACE_RECORD;
typedef struct {
    uint32_t magic,version;
    uint64_t intentdev,intentino;
    FS_REPLACE_RECORD image;
} FS_REPLACE_MARK;
static FS_READ_STATUS replace_state(const FS_READ_ROOT *r,int *intent,int *commit)
{
    struct stat st;*intent=*commit=0;
    if(fstatat(r->fd,FS_REPLACE_NAME,&st,AT_SYMLINK_NOFOLLOW)==0)*intent=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    if(fstatat(r->fd,FS_REPLACE_COMMIT,&st,AT_SYMLINK_NOFOLLOW)==0)*commit=1;
    else if(errno!=ENOENT)return FS_READ_DENIED;
    return FS_READ_OK;
}
static int replace_valid(const FS_REPLACE_RECORD *i)
{
    if(i->magic!=FS_REPLACE_MAGIC||i->version!=1||
       !memchr(i->target,0,sizeof(i->target))||
       !memchr(i->oldstage,0,sizeof(i->oldstage))||
       !memchr(i->newstage,0,sizeof(i->newstage))||
       !batch_name_ok(i->target)||
       !strncmp(i->target,".fsrp-",6)||!strncmp(i->target,".fsrb-",6)||
       !strcmp(i->oldstage,i->newstage)||
       strncmp(i->oldstage,".fsrp-",6)||strncmp(i->newstage,".fsrp-",6)||
       (i->olddev==i->newdev&&i->oldino==i->newino)||
       strlen(i->oldstage)!=38||strlen(i->newstage)!=38||
       strspn(i->oldstage+6,"0123456789abcdef")!=32||
       strspn(i->newstage+6,"0123456789abcdef")!=32)return 0;
    return 1;
}
static int replace_read(int root,const char *name,void *out,size_t size,
                        struct stat *st)
{
    int fd=openat(root,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    int ok=fd>=0&&fstat(fd,st)==0&&S_ISREG(st->st_mode)&&
       st->st_nlink>=1&&st->st_nlink<=2&&(st->st_mode&077)==0&&st->st_size==(off_t)size&&
       read(fd,out,size)==(ssize_t)size;
    if(fd>=0)close(fd);
    return ok;
}
static int replace_id(const struct stat *st,uint64_t dev,uint64_t ino)
{ return S_ISREG(st->st_mode)&&st->st_dev==(dev_t)dev&&st->st_ino==(ino_t)ino; }
/* A named stage or temporary link may already have been removed in cleanup.
   A present name is *always* checked before removal; unknown names fail shut. */
static int replace_name_check(int dir,const char *name,uint64_t dev,uint64_t ino,
                              int *present)
{
    struct stat st;
    if(fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)==0){
        *present=1;return replace_id(&st,dev,ino);
    }
    *present=0;return errno==ENOENT;
}
#if defined(FS_JREC_MUTANT_STAGE) || defined(FS_JREC_MUTANT_TEMP)
static int replace_name_check_noid(int dir,const char *name,int *present)
{
    struct stat st;
    *present=fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)==0;
    return *present||errno==ENOENT;
}
#endif
static void replace_temp_names(const FS_REPLACE_RECORD *i,char *pub,char *rb)
{
    snprintf(pub,48,".fsrp-p%.31s",i->newstage+6);
    snprintf(rb,48,".fsrb-%.32s",i->newstage+6);
}
static FS_READ_STATUS replace_recover_locked(const FS_READ_ROOT *r)
{
    FS_REPLACE_RECORD i={0},record={0};FS_REPLACE_MARK m={0};
    struct stat rec,mark,target;int present=0,commit=0,dir=-1;
    int oldstage=0,newstage=0,pub=0,rb=0,exists=0;
    const char *leaf;char publish[48],rollback[48];
    FS_READ_STATUS s=replace_state(r,&present,&commit);
    if(s!=FS_READ_OK||(!present&&!commit))return s;
    if(commit){
        if(!replace_read(r->fd,FS_REPLACE_COMMIT,&m,sizeof(m),&mark)||
           m.magic!=FS_REPLACE_MARK_MAGIC||m.version!=1||
           !replace_valid(&m.image)||!m.intentdev||!m.intentino)
            return FS_READ_DENIED;
        i=m.image;
    }
    if(present){
        if(!replace_read(r->fd,FS_REPLACE_NAME,&record,sizeof(record),&rec)||
           !replace_valid(&record))return FS_READ_DENIED;
        if(commit&&(memcmp(&record,&i,sizeof(i))||
            rec.st_dev!=(dev_t)m.intentdev||rec.st_ino!=(ino_t)m.intentino))
            return FS_READ_DENIED;
        if(!commit)i=record;
    }
    replace_temp_names(&i,publish,rollback);
    s=move_parent(r,i.target,&dir,&leaf);if(s!=FS_READ_OK)return s;
#if defined(FS_JREC_MUTANT_STAGE)
    /* Test only (test_fs_journal_fuzz_mc_stage): the identity of the two named stages is not checked. */
#define FS_NAME_CHECK_STAGE(d,n,dev,ino,p) replace_name_check_noid(d,n,p)
#else
#define FS_NAME_CHECK_STAGE(d,n,dev,ino,p) replace_name_check(d,n,dev,ino,p)
#endif
#if defined(FS_JREC_MUTANT_TEMP)
    /* Test only (test_fs_journal_fuzz_mc_temp): the identity of the publish and rollback links is not checked. */
#define FS_NAME_CHECK_TEMP(d,n,dev,ino,p) replace_name_check_noid(d,n,p)
#else
#define FS_NAME_CHECK_TEMP(d,n,dev,ino,p) replace_name_check(d,n,dev,ino,p)
#endif
    if(!FS_NAME_CHECK_STAGE(r->fd,i.oldstage,i.olddev,i.oldino,&oldstage)||
       !FS_NAME_CHECK_STAGE(r->fd,i.newstage,i.newdev,i.newino,&newstage)||
       !FS_NAME_CHECK_TEMP(dir,publish,i.newdev,i.newino,&pub)||
       !FS_NAME_CHECK_TEMP(dir,rollback,i.olddev,i.oldino,&rb)){
        s=FS_READ_DENIED;goto done;
    }
    exists=fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)==0;
    if(!exists||!replace_id(&target,commit?i.newdev:i.olddev,
                             commit?i.newino:i.oldino)){
        if(!exists||commit||!replace_id(&target,i.newdev,i.newino)){
            s=FS_READ_DENIED;goto done;
        }
    }
    if(!commit&&(!oldstage||!newstage)) {s=FS_READ_DENIED;goto done;}
    /* The target identity is validated before every rename. The cooperating
       lock serializes participants; outside actors ignoring it are excluded. */
    if(!commit&&replace_id(&target,i.newdev,i.newino)){
        if(!rb){
            if(linkat(r->fd,i.oldstage,dir,rollback,0)<0){s=FS_READ_IO;goto done;}
            crash_point(44); /* rollback link exists, target new */
        }
        if(fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)<0||
           !replace_id(&target,i.newdev,i.newino)){s=FS_READ_DENIED;goto done;}
        if(renameat(dir,rollback,dir,leaf)<0){s=FS_READ_IO;goto done;}
        rb=0;
        crash_point(45); /* rollback rename visible, parent unsynced */
    }
    if(fsync(dir)<0){s=FS_READ_IO;goto done;}
    if(!commit)crash_point(46); /* rollback synced, intent remains */
    /* Commit marker is self-contained. Once the intent is gone, stage names
       are optional: a crash may interrupt their checked cleanup. */
    if(present){
        if(!durable_remove(r->fd,FS_REPLACE_NAME)){s=FS_READ_IO;goto done;}
        crash_point(commit?49:47); /* committed marker-only or rollback orphan */
    }
    if(pub&&!durable_remove(dir,publish)){s=FS_READ_IO;goto done;}
    if(rb&&!durable_remove(dir,rollback)){s=FS_READ_IO;goto done;}
    if(oldstage&&!durable_remove(r->fd,i.oldstage)){s=FS_READ_IO;goto done;}
    crash_point(commit?54:55); /* staged cleanup partly complete */
    if(newstage&&!durable_remove(r->fd,i.newstage)){s=FS_READ_IO;goto done;}
#ifndef FS_JREC_MUTANT_TMP
    /* The create/replace temporary links of the journal record and of the marker
       (.fstxn-<hex>, .fstxn-c<hex>). Each is removed only when it is provably the
       journal's own link: same inode as the record, or the marker (read back and
       equal to the record). Anything else is left alone, as before. Best effort. */
    {
        char it[48],mt[48];struct stat t;
        uint64_t idev=present?(uint64_t)rec.st_dev:m.intentdev,
                 iino=present?(uint64_t)rec.st_ino:m.intentino;
        snprintf(it,sizeof(it),".fstxn-%.32s",i.newstage+6);
        snprintf(mt,sizeof(mt),".fstxn-c%.31s",i.newstage+6);
        if(fstatat(r->fd,it,&t,AT_SYMLINK_NOFOLLOW)==0&&replace_id(&t,idev,iino)&&
           t.st_nlink<=2)(void)durable_remove(r->fd,it);
        if(fstatat(r->fd,mt,&t,AT_SYMLINK_NOFOLLOW)==0){
            int ok=0;
            if(commit)ok=replace_id(&t,(uint64_t)mark.st_dev,(uint64_t)mark.st_ino)&&t.st_nlink<=2;
            else{
                FS_REPLACE_MARK tm={0};struct stat ts;
                ok=replace_read(r->fd,mt,&tm,sizeof(tm),&ts)&&
                   tm.magic==FS_REPLACE_MARK_MAGIC&&tm.version==1&&
                   replace_valid(&tm.image)&&tm.image.olddev==i.olddev&&tm.image.oldino==i.oldino&&
                   tm.image.newdev==i.newdev&&tm.image.newino==i.newino&&
                   !strcmp(tm.image.target,i.target)&&!strcmp(tm.image.oldstage,i.oldstage)&&
                   !strcmp(tm.image.newstage,i.newstage)&&tm.intentdev==idev&&tm.intentino==iino;
            }
            if(ok)(void)durable_remove(r->fd,mt);
        }
    }
#endif
    if(commit){
        crash_point(50); /* marker only, stage cleanup completed */
        if(!durable_remove(r->fd,FS_REPLACE_COMMIT)){s=FS_READ_IO;goto done;}
    }
    s=FS_READ_OK;
done:close(dir);return s;
}
FS_READ_STATUS FsReplaceRecover(const FS_READ_ROOT *r)
{
    int lock=-1,pending=0,batch=0,bc=0,rem=0,rc=0,mv=0,mc=0;
    FS_READ_STATUS s;
    if(!r)return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);
    if(s==FS_READ_OK)s=batch_state(r,&batch,&bc);
    if(s==FS_READ_OK)s=remove_state(r,&rem,&rc);
    if(s==FS_READ_OK)s=move_state(r,&mv,&mc);
    if(s==FS_READ_OK)s=(pending||batch||bc||rem||rc||mv||mc)?
        FS_READ_DENIED:replace_recover_locked(r);
    unlock_workspace(lock);return s;
}
FS_READ_STATUS FsReplaceFile(const FS_READ_ROOT *r,const char *path,
 const void *expected,size_t expected_len,const void *replacement,size_t replacement_len)
{
    FS_REPLACE_RECORD i={0};FS_REPLACE_MARK marker={0};
    FS_READ_META meta;unsigned char *bytes=NULL;size_t len=0;
    unsigned char nonce[16];struct stat old,st,record_st,target;
    const char *leaf;char publish[48],rollback[48],inttmp[48],marktmp[48];
    int lock=-1,dir=-1,oldfd=-1,newfd=-1,randomfd=-1,record=-1,mark=-1;
    int oldstage=0,newstage=0,journal=0,published=0,committed=0;
    int p=0,b=0,bc=0,rem=0,rc=0,mv=0,mc=0,rp=0,rpc=0;
    FS_READ_STATUS s=FS_READ_IO;
    if(!r||!path||!expected||(!replacement&&replacement_len)||
       expected_len>FS_READ_MAX||replacement_len>FS_READ_MAX||
       strlen(path)>=FS_INTENT_MAX_PATH||!batch_name_ok(path)||
       !strncmp(path,".fsrp-",6)||!strncmp(path,".fsrb-",6))return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&p);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&b,&bc);if(s!=FS_READ_OK)goto done;
    s=remove_state(r,&rem,&rc);if(s!=FS_READ_OK)goto done;
    s=move_state(r,&mv,&mc);if(s!=FS_READ_OK)goto done;
    s=replace_state(r,&rp,&rpc);if(s!=FS_READ_OK)goto done;
    if(p||b||bc||rem||rc||mv||mc||rp||rpc){s=FS_READ_DENIED;goto done;}
    s=FsReadFile(r,path,&bytes,&len,&meta);if(s!=FS_READ_OK)goto done;
    if(len!=expected_len||(len&&memcmp(bytes,expected,len))){s=FS_READ_DENIED;goto done;}
    s=posix_open(r,path,&oldfd);if(s!=FS_READ_OK)goto done;
    if(fstat(oldfd,&old)<0||!S_ISREG(old.st_mode)||old.st_nlink!=1){s=FS_READ_DENIED;goto done;}
    s=move_parent(r,path,&dir,&leaf);if(s!=FS_READ_OK)goto done;
    if(fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)<0||
       !replace_id(&target,(uint64_t)old.st_dev,(uint64_t)old.st_ino)||
       target.st_nlink!=1){s=FS_READ_DENIED;goto done;}
    randomfd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(randomfd<0||read(randomfd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
        s=FS_READ_IO;goto done;
    }
    i.magic=FS_REPLACE_MAGIC;i.version=1;
    i.olddev=(uint64_t)old.st_dev;i.oldino=(uint64_t)old.st_ino;
    strcpy(i.target,path);memcpy(i.oldstage,".fsrp-",6);
    for(size_t k=0;k<sizeof(nonce);k++)sprintf(i.oldstage+6+2*k,"%02x",nonce[k]);
    i.oldstage[38]=0;
    if(read(randomfd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
        s=FS_READ_IO;goto done;
    }
    memcpy(i.newstage,".fsrp-",6);
    for(size_t k=0;k<sizeof(nonce);k++)sprintf(i.newstage+6+2*k,"%02x",nonce[k]);
    i.newstage[38]=0;
    replace_temp_names(&i,publish,rollback);
    snprintf(inttmp,sizeof(inttmp),".fstxn-%.32s",i.newstage+6);
    snprintf(marktmp,sizeof(marktmp),".fstxn-c%.31s",i.newstage+6);
    if(linkat(dir,leaf,r->fd,i.oldstage,0)<0){s=error_status();goto done;}
    oldstage=1;
    newfd=openat(r->fd,i.newstage,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(newfd<0){s=error_status();goto done;}
    newstage=1;
    if(!write_all(newfd,replacement,replacement_len)||
       fchmod(newfd,old.st_mode&0777)<0||fsync(newfd)<0||
       fstat(newfd,&st)<0||fsync(oldfd)<0||fsync(r->fd)<0){s=FS_READ_IO;goto done;}
    i.newdev=(uint64_t)st.st_dev;i.newino=(uint64_t)st.st_ino;
    crash_point(40); /* two pinned images, no journal */
    record=openat(r->fd,inttmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(record<0){s=error_status();goto done;}
    if(!write_all(record,(const unsigned char*)&i,sizeof(i))||
       fsync(record)<0||fstat(record,&record_st)<0){s=FS_READ_IO;goto done;}
    if(linkat(r->fd,inttmp,r->fd,FS_REPLACE_NAME,0)<0){s=error_status();goto done;}
    journal=1;
    if(fsync(r->fd)<0){s=FS_READ_PENDING;goto done;}
    crash_point(41); /* durable intent, old target */
    if(linkat(r->fd,i.newstage,dir,publish,0)<0){s=error_status();goto done;}
    crash_point(42); /* publish link, old target */
    if(fstatat(dir,leaf,&target,AT_SYMLINK_NOFOLLOW)<0||
       !replace_id(&target,i.olddev,i.oldino)){s=FS_READ_DENIED;goto done;}
    if(renameat(dir,publish,dir,leaf)<0){s=error_status();goto done;}
    published=1;
    crash_point(43); /* new target, parent unsynced */
#ifdef FS_CREATE_TEST_CRASH
    if(getenv("FS_REPLACE_TEST_FAIL_SYNC")){errno=EIO;s=FS_READ_PENDING;goto done;}
#endif
    if(fsync(dir)<0){s=FS_READ_PENDING;goto done;}
    crash_point(48); /* new target synced, no marker */
    marker.magic=FS_REPLACE_MARK_MAGIC;marker.version=1;
    marker.intentdev=(uint64_t)record_st.st_dev;marker.intentino=(uint64_t)record_st.st_ino;
    marker.image=i;
    mark=openat(r->fd,marktmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(mark<0){s=FS_READ_PENDING;goto done;}
    if(!write_all(mark,(const unsigned char*)&marker,sizeof(marker))||fsync(mark)<0){
        s=FS_READ_PENDING;goto done;
    }
    crash_point(52); /* complete marker temp, no commit name */
    if(linkat(r->fd,marktmp,r->fd,FS_REPLACE_COMMIT,0)<0){s=FS_READ_PENDING;goto done;}
    crash_point(53); /* complete marker linked, root not synced */
    /* An existing marker already selects commit on process-crash replay.
       A failed durability flush after publication remains pending/unknown. */
    if(fsync(r->fd)<0){s=FS_READ_PENDING;goto done;}
    committed=1;
    crash_point(51); /* durable commit, cleanup pending */
    (void)replace_recover_locked(r);
    s=FS_READ_OK;
done:
    if(mark>=0)close(mark);
    if(record>=0)close(record);
    if(oldfd>=0)close(oldfd);
    if(newfd>=0)close(newfd);
    if(dir>=0)close(dir);
    if(randomfd>=0)close(randomfd);
    free(bytes);
    if(i.newstage[0]){
        /* Random pre-publication temp records are never replay inputs. */
        (void)unlinkat(r->fd,inttmp,0);
        (void)unlinkat(r->fd,marktmp,0);
        (void)fsync(r->fd);
    }
    if(!journal){
        if(oldstage)(void)durable_remove(r->fd,i.oldstage);
        if(newstage)(void)durable_remove(r->fd,i.newstage);
    }else if(!published&&!committed){
        /* Failure after the journal, target still the old inode: roll back
           now instead of leaving a pending intent. A failed rollback leaves
           the journal and reports IO. */
        if(replace_recover_locked(r)!=FS_READ_OK)s=FS_READ_IO;
    }
    unlock_workspace(lock);
    return committed?FS_READ_OK:(published?FS_READ_PENDING:s);
}
#endif
