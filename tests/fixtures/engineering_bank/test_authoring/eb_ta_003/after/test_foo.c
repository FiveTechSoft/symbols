#include "util.h"

int main(void) {
    return clamp(0, 1, 10) == 1 ? 0 : 1;
}
