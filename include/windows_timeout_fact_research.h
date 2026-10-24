#ifndef WINDOWS_TIMEOUT_FACT_RESEARCH_H
#define WINDOWS_TIMEOUT_FACT_RESEARCH_H
/* One reviewed source-specific adapter, not a generic fact engine. */
#define WINDOWS_TIMEOUT_FACT_SHA "6f59b9bd7a7e1dacb2bbc5d93900edad45a48483d20ce90be3683ad40f23b437"
#define WINDOWS_TIMEOUT_SOURCE_URL "https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/timeout"
int WindowsTimeoutFactMatches(const char *command,long requested,
                              const char *qualifier,int *nobreak);
#endif
