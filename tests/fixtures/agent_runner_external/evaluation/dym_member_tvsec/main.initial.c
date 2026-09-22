struct ext_ts { long tv_sec; long tv_nsec; }; long probe_dym_member(void) { struct ext_ts t; t.tv_sec = 0; return t.tv_sec; }
