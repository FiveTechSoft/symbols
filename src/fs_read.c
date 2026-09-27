#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_read.h"
#include "fs_write.h"
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
static int write_all(int fd,const unsigned char *bytes,size_t len)
{
    size_t used=0;
    while(used<len){ssize_t n=write(fd,bytes+used,len-used);
        if(n<=0)return 0;
        used+=(size_t)n;}
    return 1;
}
FS_READ_STATUS FsCreateFile(const FS_READ_ROOT *r,const char *rel,
                            const void *bytes,size_t len,unsigned mode)
{
    char *parent=NULL,*slash,*leaf;int dir=-1,temp=-1,random_fd=-1,lockfd=-1;
    char temp_name[48]={0};unsigned char nonce[16];FS_READ_STATUS s=FS_READ_IO;
    FS_MANIFEST plan={0};FS_OP_REQUEST request={FS_OP_CREATE,NULL,rel};
    if(!r||!valid_relative(rel,0)||(!bytes&&len)||len>FS_READ_MAX||mode>0777)
        return FS_READ_INVALID;
    /* Keep control names unavailable to application writes. */
    if(!strcmp(rel,".fstxn.lock")||!strncmp(rel,".fstxn/",7)||
       !strcmp(rel,".fstxn"))return FS_READ_DENIED;
    s=lock_workspace(r,&lockfd);if(s!=FS_READ_OK)return s;
    s=FsManifestPlan(r,&request,1,&plan);
    if(s!=FS_READ_OK)goto done;
    FsManifestFree(&plan);
    parent=(char*)malloc(strlen(rel)+1);if(!parent){s=FS_READ_IO;goto done;}
    strcpy(parent,rel);slash=strrchr(parent,'/');
    if(slash){*slash=0;leaf=slash+1;}else{*parent=0;leaf=(char*)rel;}
    s=posix_open(r,parent,&dir);if(s!=FS_READ_OK)goto done;
    {FS_READ_META meta;s=posix_meta(dir,&meta);
     if(s!=FS_READ_OK||meta.kind!=FS_KIND_DIR){s=FS_READ_UNSUPPORTED;goto done;}}
    /* A kernel-generated nonce prevents predictable temporary siblings. */
    random_fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random_fd<0||read(random_fd,nonce,sizeof(nonce))!=(ssize_t)sizeof(nonce)){
        s=FS_READ_IO;goto done;
    }
    close(random_fd);random_fd=-1;
    memcpy(temp_name,".fst-",5);
    for(size_t i=0;i<sizeof(nonce);i++)sprintf(temp_name+5+i*2,"%02x",nonce[i]);
    temp_name[37]=0;
    temp=openat(dir,temp_name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(temp<0){s=error_status();goto done;}
    if(!write_all(temp,(const unsigned char*)bytes,len) ||
       fchmod(temp,(mode_t)mode)<0 || fsync(temp)<0){
        s=FS_READ_IO;goto done;
    }
    /* No-replace publish: linkat refuses an existing target atomically. */
    {FS_READ_META current;FS_READ_STATUS check=FsReadStat(r,rel,&current);
     if(check!=FS_READ_MISSING){s=check==FS_READ_OK?FS_READ_DENIED:check;goto done;}}
    if(linkat(dir,temp_name,dir,leaf,0)<0){
        s=(errno==EEXIST)?FS_READ_DENIED:error_status();goto done;
    }
    /* Publish has completed. Remove the temporary sibling. */
    if(unlinkat(dir,temp_name,0)==0)temp_name[0]=0;
    /* A cleanup failure leaves an extra hard link, but the requested name
       is already published. Return success rather than invite duplicate work. */
    /* The file was published; directory fsync only strengthens durability.
       Do not report failure after a visible publish and invite an unsafe retry. */
    (void)fsync(dir);
    s=FS_READ_OK;
done:
    if(temp>=0)close(temp);
    if(dir>=0){if(temp_name[0] && temp>=0)unlinkat(dir,temp_name,0);
        close(dir);}
    if(random_fd>=0)close(random_fd);
    free(parent);unlock_workspace(lockfd);return s;
}
#endif
