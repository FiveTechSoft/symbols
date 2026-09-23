int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '-' && argv[i][1] == '-' ) { /* --debug */ }
    return 0;
}
