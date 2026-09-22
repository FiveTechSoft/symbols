#ifndef INI_H
#define INI_H
typedef int (*ini_handler)(void *user, const char *section, const char *name, const char *value);
int ini_parse(const char *filename, ini_handler handler, void *user);
#endif
