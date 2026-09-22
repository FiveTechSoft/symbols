#ifndef JANSSON_TYPES_H
#define JANSSON_TYPES_H
typedef struct json_t {
    int type;
    unsigned long refcount;
} json_t;
#endif
