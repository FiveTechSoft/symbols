Buffer too small: the buffer must hold 6 chars + NUL (char buf[7] at least). Currently buf[4]. Fix the size so strcpy-like initialization is valid and the program exits 0. Forbidden: 'char buf[4]'.
