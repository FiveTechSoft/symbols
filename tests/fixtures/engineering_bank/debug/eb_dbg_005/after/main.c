int main(void) {
    int total = 0;
    for (int i = 1; i <= 3; i++) total += i;
    return total == 6 ? 0 : 1;
}
