static int is_valid(int v) {
    return v >= 0 && v <= 99;
}
int main(void) {
    return is_valid(99) ? 0 : 1;
}
