#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_read.h"
#include "fs_write.h"
#include "fs_batch.h"
#include "fs_remove.h"
#include "fs_move.h"
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
static int valid_relative(const char *p, int allow_empty)
{
    const char *start;
    if (!p || (!*p && !allow_empty) || *p == '/' || *p == '\\') return 0;
    if (!*p) return 1;
    for (start=p; *p; p++) {
        if (*p == '\\' || *p == ':' || (unsigned char)*p < 32) return 0;
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
    if(i.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)return FS_READ_DENIED;
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
        child=openat(fd,part,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW|(next?O_DIRECTORY:0));
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
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *r,const char *rel,
                            const void *bytes,size_t len,unsigned mode)
{
    if(!r||!valid_relative(rel,0)||(!bytes&&len)||len>FS_READ_MAX||mode>0777)
        return FS_READ_INVALID;
    /* No path-by-name fallback: handle-relative Windows publish is pending. */
    return FS_READ_UNSUPPORTED;
}
FS_READ_STATUS FsCreateRecover(const FS_READ_ROOT *r)
{ return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsCopyFile(const FS_READ_ROOT *r,const char *src,const char *dst,
                          const void *expected,size_t len)
{ (void)src;(void)dst;(void)expected;(void)len;
  return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsBatchCreate(const FS_READ_ROOT *r,const FS_BATCH_CREATE *e,size_t n)
{ (void)e;(void)n;return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsBatchRecover(const FS_READ_ROOT *r)
{ return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsRemoveFile(const FS_READ_ROOT *r,const char *path,
                            const void *expected,size_t len)
{ (void)path;(void)expected;(void)len;return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsRemoveRecover(const FS_READ_ROOT *r)
{ return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsMoveFile(const FS_READ_ROOT *r,const char *src,const char *dst,
                          const void *expected,size_t len)
{ (void)src;(void)dst;(void)expected;(void)len;
  return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
FS_READ_STATUS FsMoveRecover(const FS_READ_ROOT *r)
{ return r?FS_READ_UNSUPPORTED:FS_READ_INVALID; }
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
        strcmp(i->target,".fstxn") &&
        strncmp(i->target,".fstxn/",7) &&
        strncmp(i->target,".fst-",5) &&
        strncmp(i->target,".fstxn-",7);
}
static FS_READ_STATUS batch_state(const FS_READ_ROOT *r,int *batch,int *commit);
static FS_READ_STATUS remove_state(const FS_READ_ROOT *r,int *intent,int *commit);
static FS_READ_STATUS move_state(const FS_READ_ROOT *r,int *intent,int *commit);
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
    unsigned char nonce[16];FS_READ_STATUS s=FS_READ_IO;int published=0,pending=0;
    struct stat st;FS_CREATE_INTENT intent={0};
    FS_MANIFEST plan={0};FS_OP_REQUEST request={FS_OP_CREATE,NULL,rel};
    if(!r||!valid_relative(rel,0)||strlen(rel)>=sizeof(parent)||
       (!bytes&&len)||len>FS_READ_MAX||mode>0777)return FS_READ_INVALID;
    if(!strcmp(rel,".fstxn.lock")||!strcmp(rel,FS_INTENT_NAME)||
       !strcmp(rel,FS_BATCH_NAME)||!strcmp(rel,FS_BATCH_COMMIT)||
       !strcmp(rel,FS_REMOVE_NAME)||!strcmp(rel,FS_REMOVE_COMMIT)||
       !strcmp(rel,FS_MOVE_NAME)||!strcmp(rel,FS_MOVE_COMMIT)||
       !strncmp(rel,".fstxn/",7)||!strcmp(rel,".fstxn")||
       !strncmp(rel,".fst-",5)||!strncmp(rel,".fstxn-",7)||
       !strncmp(rel,".fsrm-",6)||!strncmp(rel,".fsmv-",6))return FS_READ_DENIED;
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
    pending=1;
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
      strcmp(p,".fstxn") && strncmp(p,".fstxn/",7) &&
      strncmp(p,".fst-",5) && strncmp(p,".fstxn-",7) &&
      strncmp(p,".fsrm-",6) && strncmp(p,".fsmv-",6);
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
      if(s==FS_READ_OK)s=(pending||removal||committed||mv||mc)?
          FS_READ_DENIED:batch_recover_locked(r);
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
    unlock_workspace(lock);
    /* If any name became visible, report an interrupted transaction as IO,
       never invite blind retry. Caller must recover explicitly. */
    return committed_here?FS_READ_OK:(visible?FS_READ_IO:s);
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
       !strncmp(dst,".fstxn/",7)||!strcmp(dst,".fstxn")||
       !strncmp(dst,".fst-",5)||!strncmp(dst,".fstxn-",7)||
       !strncmp(dst,".fsrm-",6)||!strncmp(dst,".fsmv-",6))return FS_READ_DENIED;
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
      if(s==FS_READ_OK)s=(pending||batch||commit||mv||mc)?
          FS_READ_DENIED:remove_recover_locked(r);
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
       !strcmp(path,FS_MOVE_COMMIT)||!strncmp(path,".fsmv-",6))return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&batch,&committed);if(s!=FS_READ_OK)goto done;
    s=remove_state(r,&removal,&rcommit);if(s!=FS_READ_OK)goto done;
    if(pending||batch||committed||removal||rcommit){s=FS_READ_DENIED;goto done;}
    {int mv=0,mc=0;
     s=move_state(r,&mv,&mc);
     if(s!=FS_READ_OK||mv||mc){s=s==FS_READ_OK?FS_READ_DENIED:s;goto done;}
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
       !strcmp(i->target,FS_MOVE_NAME)||!strcmp(i->target,FS_MOVE_COMMIT)||
       !strncmp(i->source,".fsrm-",6)||!strncmp(i->target,".fsrm-",6)||
       !strncmp(i->source,".fsmv-",6)||!strncmp(i->target,".fsmv-",6)||
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
    if(s==FS_READ_OK)s=(pending||batch||commit||remove||rc)?
        FS_READ_DENIED:move_recover_locked(r);
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
       !strncmp(src,".fsmv-",6)||!strncmp(dst,".fsmv-",6)||
       !strncmp(src,".fsrm-",6)||!strncmp(dst,".fsrm-",6))return FS_READ_INVALID;
    s=lock_workspace(r,&lock);if(s!=FS_READ_OK)return s;
    s=pending_intent(r,&pending);if(s!=FS_READ_OK)goto done;
    s=batch_state(r,&batch,&bc);if(s!=FS_READ_OK)goto done;
    s=remove_state(r,&remove,&rc);if(s!=FS_READ_OK)goto done;
    s=move_state(r,&mv,&mc);if(s!=FS_READ_OK)goto done;
    if(pending||batch||bc||remove||rc||mv||mc){s=FS_READ_DENIED;goto done;}
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
    unlock_workspace(lock);
    return marked?FS_READ_OK:((target_visible||source_removed)?FS_READ_IO:s);
}
#endif
