#include "powershell_sleep_fact_research.h"
#include <stdio.h>
#include <string.h>
int PowerShellSleepFactMatches(const char *command,long requested,const char *qualifier){
 if(requested<0||requested>99||strcmp(qualifier,"default"))return 0;
 char expected[64];snprintf(expected,sizeof(expected),"Start-Sleep -Seconds %ld",requested);
 return !strcmp(command,expected);
}
