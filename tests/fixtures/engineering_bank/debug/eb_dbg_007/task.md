Buffer too small: the buffer must hold 6 chars + NUL (char buf[7] at least). Currently buf[4]. Fix the size. Forbidden: 'char buf[4]'.
