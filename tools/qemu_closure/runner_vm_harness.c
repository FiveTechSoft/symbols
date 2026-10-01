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

static void fail(void) { _exit(2); }
static void bound(int kind, rlim_t value) {
    struct rlimit current, next = {value, value};
    if (getrlimit(kind, &current) ||
        (current.rlim_max != RLIM_INFINITY && current.rlim_max < value) ||
        setrlimit(kind, &next) || getrlimit(kind, &current) ||
        current.rlim_cur != value || current.rlim_max != value) fail();
}
#define ALLOW(n) BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, (n), 0, 1), BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW)
int main(void) {
#if !defined(__x86_64__) || defined(__ILP32__)
    fail();
#else
    if (getuid() == 0 || geteuid() == 0) fail();
    /* close_range has no fallback: unsupported or unknown means refusal. */
    if (syscall(SYS_close_range, 3U, UINT_MAX, 0U)) fail();
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
    struct rlimit observed;
    const int kinds[] = {RLIMIT_AS,RLIMIT_CPU,RLIMIT_FSIZE,RLIMIT_NOFILE,RLIMIT_CORE};
    const rlim_t values[] = {64U*1024U*1024U,1,1024,16,0};
    for (unsigned int i=0;i<sizeof(kinds)/sizeof(kinds[0]);++i)
        if (syscall(SYS_getrlimit,kinds[i],&observed) ||
            observed.rlim_cur!=values[i] || observed.rlim_max!=values[i]) fail();
    /* Ceiling raises, not resource-consumption stress. Keep the filter unchanged. */
    for (unsigned int i=0;i<sizeof(kinds)/sizeof(kinds[0]);++i) {
        struct rlimit raise = {values[i]+1,values[i]+1}; errno=0;
        if (syscall(SYS_setrlimit,kinds[i],&raise) != -1 || errno != EPERM) fail();
        if (syscall(SYS_getrlimit,kinds[i],&observed) ||
            observed.rlim_cur!=values[i] || observed.rlim_max!=values[i]) fail();
    }
    static const char output[] = "{\"schema\":\"symbols.runner-vm-harness-child.v2\",\"classification\":\"measured_only\",\"limits_readback\":true,\"all_five_ceiling_raises_denied\":true,\"allow_getpid\":true,\"deny_classes\":true}\n";
    if (syscall(SYS_write,1,output,sizeof(output)-1) != (long)(sizeof(output)-1)) fail();
    syscall(SYS_exit_group,0);
#endif
    fail();
    return 2;
}
