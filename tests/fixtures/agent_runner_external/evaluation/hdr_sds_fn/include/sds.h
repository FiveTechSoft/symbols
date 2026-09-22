#ifndef SDS_H
#define SDS_H
typedef char *sds;
sds sdsnewlen(const void *init, unsigned long initlen);
#endif
