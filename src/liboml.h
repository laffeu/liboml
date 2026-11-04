#ifndef LIBOML_H_
#define LIBOML_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *key, *value;
    char **array;
} entry;

typedef struct {
    char *name;
    entry *entries;
    size_t size, capacity, parent;
} section;

typedef struct {
    section *sections;
    size_t size, capacity;
} table;

table *oml_get(const char *filename);

const section *oml_section(table *t, char *name);

const section *oml_nested(table *t, section *parent,
                          char *name);
const entry *oml_value(section *s, char *key);

void oml_free(table *t);

#ifdef __cplusplus
}
#endif

#endif