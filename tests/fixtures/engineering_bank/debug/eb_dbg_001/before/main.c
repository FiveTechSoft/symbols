int main(void) {
    int a[4] = {-1, -1, -1, 12345};
    int n = 3;
    for (int i = 0; i <= n; i++) {
        a[i] = i;
    }
    return (a[3] == 12345 && a[0] == 0 && a[1] == 1 && a[2] == 2) ? 0 : 1;
}
