main.c calls get() but does not include util.h and has no local prototype. Add the include so the program compiles with -Werror=implicit-function-declaration and exits 0.
