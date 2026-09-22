typedef struct { long sec; } tthread_timespec; long probe_dym_tspec(void) { tthread_timespec ts; ts.sec = 0; return ts.sec; }
