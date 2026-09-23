static int process(int x) { return x + 1; }

int main(void) {
    return process(1) == 2 ? 0 : 1;
}
