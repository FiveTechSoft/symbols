static int scale(const int *p, int k) {
    return *p * k;
}

int main(void) {
    int v = 2;
    return scale(&v, 3) == 6 ? 0 : 1;
}
