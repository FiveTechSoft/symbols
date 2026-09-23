The include guard in util.h is wrong (uses MAIN_H). Change it to UTIL_H in both #ifndef and #define so the program still exits 0 and the guard names match.
