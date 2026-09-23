build.sh must fail closed: if the compiler fails, the script must exit non-zero. The checker writes invalid C to main.c and runs build.sh; before/ hardcodes success, after/ must exit non-zero.
