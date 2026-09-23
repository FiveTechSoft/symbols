int clamp_upper(int v, int hi) {
    if (v > hi) return hi;
    return v;
}
int main(void) { return clamp_upper(20, 10) == 10 ? 0 : 1; }
