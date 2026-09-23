int main(void) {
    int ok = 1;
    if (!ok) goto fail;
    return 0;
fail:
    return 1;
}
