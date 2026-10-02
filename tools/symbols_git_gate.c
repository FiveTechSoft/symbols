#include "git_gate.h"

int main(int argc, char **argv)
{
    return GitGateRun(argc, argv, stdout, stderr);
}
