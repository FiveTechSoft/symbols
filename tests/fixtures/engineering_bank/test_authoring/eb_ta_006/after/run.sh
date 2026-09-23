#!/bin/sh
set -e
gcc -std=c11 -Werror=implicit-function-declaration -o test_foo_bin test_foo.c util.c
./test_foo_bin
