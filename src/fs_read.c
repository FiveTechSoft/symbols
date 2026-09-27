#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "fs_read.h"
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
struct FS_READ_ROOT { HANDLE handle; };
/* Until Windows has handle-relative traversal, never interpret a child path
   through a mutable namespace. A root handle is safe to inspect directly. */
FS_READ_STATUS FsReadOpen(const char *root,FS_READ_ROOT **out)
{
    int n;wchar_t *w;HANDLE h;BY_HANDLE_FILE_INFORMATION info;FS_READ_ROOT *r;
    if(!root||!out||!*root)return FS_READ_INVALID;
    *out=NULL;n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,root,-1,NULL,0);
    if(n<=0)return FS_READ_INVALID;
    w=(wchar_t*)malloc((size_t)n*sizeof(wchar_t));if(!w)return FS_READ_IO;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,root,-1,w,n)){
        free(w);return FS_READ_INVALID;
    }
    h=CreateFileW(w,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                  NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    free(w);
    if(h==INVALID_HANDLE_VALUE)return FS_READ_IO;
    if(!GetFileInformationByHandle(h,&info) ||
       !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
       (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)){
        CloseHandle(h);return FS_READ_DENIED;
    }
    r=(FS_READ_ROOT*)malloc(sizeof(*r));if(!r){CloseHandle(h);return FS_READ_IO;}
    r->handle=h;*out=r;return FS_READ_OK;
}
void FsReadClose(FS_READ_ROOT *r){if(r){CloseHandle(r->handle);free(r);}}
FS_READ_STATUS FsReadStat(const FS_READ_ROOT *r,const char *rel,FS_READ_META *m)
{
    BY_HANDLE_FILE_INFORMATION info;
    if(!r||!m||!valid_relative(rel,1))return FS_READ_INVALID;
    if(*rel)return FS_READ_UNSUPPORTED;
    if(!GetFileInformationByHandle(r->handle,&info))return FS_READ_IO;
    if(!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
       (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))return FS_READ_DENIED;
    m->kind=FS_KIND_DIR;m->size=((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow;
    m->mode=(info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)?0444:0755;
    m->binary=-1;m->newline=FS_NEWLINE_NONE;return FS_READ_OK;
}
FS_READ_STATUS FsReadFile(const FS_READ_ROOT *r,const char *rel,unsigned char **b,size_t *n,FS_READ_META *m)
{
    if(!r||!b||!n||!m||!valid_relative(rel,0))return FS_READ_INVALID;
    *b=NULL;*n=0;return FS_READ_UNSUPPORTED;
}
FS_READ_STATUS FsReadList(const FS_READ_ROOT *r,const char *rel,FS_READ_ENTRY **e,size_t *n)
{
    if(!r||!e||!n||!valid_relative(rel,1))return FS_READ_INVALID;
    *e=NULL;*n=0;return FS_READ_UNSUPPORTED;
}

#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
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
