#ifndef LIBOML_H_
#define LIBOML_H_

#include <stddef.h>

typedef struct {
    char *key;
    char *value;
    char **array;
} entry;

typedef struct {
    char *name;
    entry *entries;
    size_t count;
} section;

typedef struct {
    section *sections;
    size_t count;
} table;

table *oml_get(const char *filename);
const entry *oml_bykey(const table *t, const char *section_name, const char *key);
void oml_free(table *t);

#endif