#ifndef SHELL_RESOURCE_TYPE_H
#define SHELL_RESOURCE_TYPE_H
#include <stddef.h>
#include <stdint.h>
/* Counted UTF-16, never strlen on a captured external buffer. */
static int resource_type_file(const void *data,size_t bytes,unsigned flags)
{
    const unsigned char *p=(const unsigned char *)data;
    static const unsigned char expected[8]={'F',0,'i',0,'l',0,'e',0};
    size_t i;
    if(!data || !(flags&1u) || (bytes!=8 && bytes!=10))return 0;
    for(i=0;i<8;i++)if(p[i]!=expected[i])return 0;
    return bytes==8 || (p[8]==0 && p[9]==0);
}
#endif
