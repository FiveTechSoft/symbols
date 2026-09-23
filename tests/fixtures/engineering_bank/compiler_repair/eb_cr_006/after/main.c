#include <stddef.h>
#include <string.h>

int main(void) {
    const char *s = "abc";
    return (int)strlen(s) - 3;
}
