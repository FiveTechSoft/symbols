#include <signal.h>
#include <stdlib.h>
int main(int c,char**v){if(c<2)return 2;raise(atoi(v[1]));return 0;}
