/* Trusted numeric-loopback positive control only. Never a workload. */
#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/syscall.h>
static void fail(void) { _exit(2); }
static const unsigned char mark[]={76,79,67,76};
/* Fixed empty-name DNS question: no external hostname/resolver. */
static const unsigned char dns[]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,0,0,1,0,1};
static void exchange(int fd,const void *data,size_t n,int method) {
    struct iovec v={(void *)data,n};struct msghdr m={.msg_iov=&v,.msg_iovlen=1};
    ssize_t sent=method==2?sendto(fd,data,n,0,NULL,0):method?sendmsg(fd,&m,0):write(fd,data,n);
    if (sent!=(ssize_t)n) fail();
    char reply;
    if (read(fd,&reply,1)!=1 || reply!='R') fail();
}
int main(int argc,char **argv) {
    if (argc!=7 || getuid()==0 || geteuid()==0) fail();
    int n[6];
    for (int i=0;i<6;i++) {
        char *end;long v=strtol(argv[i+1],&end,10);
        if (!*argv[i+1] || *end || v<3 || v>65535) fail();
        n[i]=(int)v;
    }
    if (n[4]==n[5] || n[4]>=16 || n[5]>=16) fail();
    const int kinds[]={RLIMIT_AS,RLIMIT_CPU,RLIMIT_FSIZE,RLIMIT_NOFILE,RLIMIT_CORE};
    const rlim_t values[]={64U*1024U*1024U,1,1024,16,0};
    for (unsigned int i=0;i<sizeof(kinds)/sizeof(kinds[0]);i++) {
        struct rlimit current,next={values[i],values[i]};
        if (getrlimit(kinds[i],&current) ||
            (current.rlim_max!=RLIM_INFINITY && current.rlim_max<values[i]) ||
            setrlimit(kinds[i],&next) || getrlimit(kinds[i],&current) ||
            current.rlim_cur!=values[i] || current.rlim_max!=values[i]) fail();
    }
    for (int transport=0;transport<2;transport++) for (int family=0;family<2;family++) {
        struct sockaddr_in a4={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK),.sin_port=htons((uint16_t)n[family+2*transport])};
        struct sockaddr_in6 a6={.sin6_family=AF_INET6,.sin6_addr=IN6ADDR_LOOPBACK_INIT,.sin6_port=htons((uint16_t)n[family+2*transport])};
        for (int probe=0;probe<2;probe++) {
            int fd=socket(family?AF_INET6:AF_INET,transport?SOCK_DGRAM:SOCK_STREAM,0);
            if (fd<0 || connect(fd,family?(void *)&a6:(void *)&a4,family?sizeof(a6):sizeof(a4))) fail();
            unsigned char framed[sizeof(dns)+2]={0,sizeof(dns)};memcpy(framed+2,dns,sizeof(dns));
            exchange(fd,probe?(transport?(void *)dns:(void *)framed):(void *)mark,
                     probe?(transport?sizeof(dns):sizeof(framed)):sizeof(mark),transport&&!probe?2:1);
            if (close(fd)) fail();
        }
    }
    for (int i=4;i<6;i++) {
        exchange(n[i],mark,sizeof(mark),1);
        exchange(n[i],mark,sizeof(mark),0);
    }
    if (syscall(SYS_close_range,3U,UINT_MAX,0U)) fail();
    pid_t child=fork();if (child<0) fail();if (!child) _exit(0);
    int status;if (waitpid(child,&status,0)!=child || !WIFEXITED(status) || WEXITSTATUS(status)) fail();
    static const char out[]="{\"schema\":\"symbols.runner-vm-egress-control.v1\",\"classification\":\"measured_only\",\"loopback_baselines\":true,\"inherited_baselines\":true,\"fork_reaped\":true}\n";
    if (write(1,out,sizeof(out)-1)!=(ssize_t)(sizeof(out)-1)) fail();
    _exit(0);
}
