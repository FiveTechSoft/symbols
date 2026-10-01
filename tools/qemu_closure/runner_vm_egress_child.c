/* Trusted single-process capability harness, not QEMU confinement proof. */
#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdlib.h>

static void fail(void) { _exit(2); }
static void bound(int kind, rlim_t value) {
    struct rlimit current, next = {value, value};
    if (getrlimit(kind, &current) ||
        (current.rlim_max != RLIM_INFINITY && current.rlim_max < value) ||
        setrlimit(kind, &next) || getrlimit(kind, &current) ||
        current.rlim_cur != value || current.rlim_max != value) fail();
}
#define ALLOW(n) BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, (n), 0, 1), BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW)
int main(int argc, char **argv) {
    if (argc != 7) fail();
    int value[6];
    for (int i=0;i<6;i++) {
        char *end; long n=strtol(argv[i+1],&end,10);
        if (!*argv[i+1] || *end || n<3 || n>65535) fail();
        value[i]=(int)n;
    }
    if (value[4]==value[5] || value[4]>=16 || value[5]>=16) fail();
    struct stat fdinfo;
    for (int i=4;i<6;i++)
        if (fstat(value[i],&fdinfo) || !S_ISSOCK(fdinfo.st_mode)) fail();
#if !defined(__x86_64__) || defined(__ILP32__)
    fail();
#else
    if (getuid() == 0 || geteuid() == 0) fail();
    /* close_range has no fallback: unsupported or unknown means refusal. */
    if (syscall(SYS_close_range, 3U, UINT_MAX, 0U)) fail();
    for (int i=4;i<6;i++) {
        errno=0;
        if (fcntl(value[i],F_GETFD)!=-1 || errno!=EBADF) fail();
    }
    bound(RLIMIT_AS, 64U*1024U*1024U);
    bound(RLIMIT_CPU, 1);
    bound(RLIMIT_FSIZE, 1024);
    bound(RLIMIT_NOFILE, 16);
    bound(RLIMIT_CORE, 0);
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0) != 1) fail();
    struct sock_filter code[] = {
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, nr)),
        /* Reject x32 rather than assuming AUDIT_ARCH_X86_64 is enough. */
        BPF_JUMP(BPF_JMP|BPF_JSET|BPF_K, 0x40000000U, 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL_PROCESS),
        /* write only stdout; high 32 bits must be zero. */
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, SYS_write, 0, 6),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, args[0])+4),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0, 0, 3),
        BPF_STMT(BPF_LD|BPF_W|BPF_ABS, offsetof(struct seccomp_data, args[0])),
        BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 1, 0, 1),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|EPERM),
        ALLOW(SYS_exit), ALLOW(SYS_exit_group),
        ALLOW(SYS_getpid), ALLOW(SYS_getrlimit), ALLOW(SYS_setrlimit),
        BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO|EPERM)
    };
    struct sock_fprog program = {(unsigned short)(sizeof(code)/sizeof(code[0])), code};
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program)) fail();
    if (syscall(SYS_getpid) <= 0) fail();
    /* No successful socket, fork or network interaction occurs. */
    const long denied[] = {SYS_socket, SYS_fork, SYS_vfork, SYS_clone, SYS_clone3,
                          SYS_execve, SYS_openat, SYS_ptrace, SYS_io_uring_setup};
    for (unsigned int i=0; i<sizeof(denied)/sizeof(denied[0]); ++i) {
        errno=0;
        if (syscall(denied[i], 0,0,0,0,0,0) != -1 || errno != EPERM) fail();
    }
    struct sockaddr_in a4={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    struct sockaddr_in6 a6={.sin6_family=AF_INET6,.sin6_addr=IN6ADDR_LOOPBACK_INIT};
    char marker[4]={'L','O','C','L'};
    unsigned char dns[17]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,0,0,1,0,1};
    unsigned char framed[19]={0,17};
    for (int i=0;i<17;i++) framed[i+2]=dns[i];
    struct iovec vec={marker,sizeof(marker)};
    struct msghdr msg={.msg_iov=&vec,.msg_iovlen=1};
    for (int family=0;family<2;family++) {
        void *addr=family?(void *)&a6:(void *)&a4;
        size_t size=family?sizeof(a6):sizeof(a4);
        a4.sin_port=htons((unsigned short)value[family]);
        a6.sin6_port=htons((unsigned short)value[family]);
        errno=0;
        if (syscall(SYS_socket,family?AF_INET6:AF_INET,SOCK_STREAM,0)!=-1 || errno!=EPERM) fail();
        errno=0;
        if (syscall(SYS_connect,value[4],addr,size)!=-1 || errno!=EPERM) fail();
        a4.sin_port=htons((unsigned short)value[family+2]);
        a6.sin6_port=htons((unsigned short)value[family+2]);
        errno=0;
        if (syscall(SYS_socket,family?AF_INET6:AF_INET,SOCK_DGRAM,0)!=-1 || errno!=EPERM) fail();
        errno=0;
        if (syscall(SYS_sendto,value[5],marker,sizeof(marker),0,addr,size)!=-1 || errno!=EPERM) fail();
        msg.msg_name=addr;msg.msg_namelen=(socklen_t)size;errno=0;
        if (syscall(SYS_sendmsg,value[5],&msg,0)!=-1 || errno!=EPERM) fail();
    }
    for (int family=0;family<2;family++) {
        void *addr=family?(void *)&a6:(void *)&a4;
        size_t size=family?sizeof(a6):sizeof(a4);
        a4.sin_port=htons((unsigned short)value[family+2]);a6.sin6_port=htons((unsigned short)value[family+2]);
        errno=0;
        if (syscall(SYS_sendto,value[5],dns,sizeof(dns),0,addr,size)!=-1 || errno!=EPERM) fail();
        vec.iov_base=dns;vec.iov_len=sizeof(dns);msg.msg_name=addr;msg.msg_namelen=(socklen_t)size;errno=0;
        if (syscall(SYS_sendmsg,value[5],&msg,0)!=-1 || errno!=EPERM) fail();
        a4.sin_port=htons((unsigned short)value[family]);a6.sin6_port=htons((unsigned short)value[family]);
        errno=0;
        if (syscall(SYS_connect,value[4],addr,size)!=-1 || errno!=EPERM) fail();
        vec.iov_base=framed;vec.iov_len=sizeof(framed);msg.msg_name=addr;msg.msg_namelen=(socklen_t)size;errno=0;
        if (syscall(SYS_sendmsg,value[4],&msg,0)!=-1 || errno!=EPERM) fail();
    }
    vec.iov_base=marker;vec.iov_len=sizeof(marker);
    for (int i=4;i<6;i++) {
        msg.msg_name=0;msg.msg_namelen=0;errno=0;
        if (syscall(SYS_sendmsg,value[i],&msg,0)!=-1 || errno!=EPERM) fail();
        errno=0;
        if (syscall(SYS_write,value[i],marker,sizeof(marker))!=-1 || errno!=EPERM) fail();
    }
    struct rlimit observed;
    const int kinds[] = {RLIMIT_AS,RLIMIT_CPU,RLIMIT_FSIZE,RLIMIT_NOFILE,RLIMIT_CORE};
    const rlim_t values[] = {64U*1024U*1024U,1,1024,16,0};
    for (unsigned int i=0;i<sizeof(kinds)/sizeof(kinds[0]);++i)
        if (syscall(SYS_getrlimit,kinds[i],&observed) ||
            observed.rlim_cur!=values[i] || observed.rlim_max!=values[i]) fail();
    struct rlimit raise = {17,17}; errno=0;
    if (syscall(SYS_setrlimit,RLIMIT_NOFILE,&raise) != -1 || errno != EPERM) fail();
    static const char output[] = "{\"schema\":\"symbols.runner-vm-egress-child.v1\",\"classification\":\"measured_only\",\"limits_readback\":true,\"hard_raise_denied\":true,\"allow_getpid\":true,\"deny_classes\":true,\"known_fds_closed\":true,\"transport_samples\":true}\n";
    if (syscall(SYS_write,1,output,sizeof(output)-1) != (long)(sizeof(output)-1)) fail();
    syscall(SYS_exit_group,0);
#endif
    fail();
    return 2;
}
