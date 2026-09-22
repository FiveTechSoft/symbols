#ifndef CJSON_UTILS_H
#define CJSON_UTILS_H
typedef struct cJSON { int type; } cJSON;
int cJSONUtils_Compare(const cJSON *a, const cJSON *b);
#endif
