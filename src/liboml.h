#ifndef LIBOML_H_
#define LIBOML_H_

#include <stdbool.h>
#include <stddef.h>

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

const section *oml_section(table *t, const char *name);
const section *oml_nested(table *t, const section *parent,
                          const char *name);

const entry *oml_value(const section *s, const char *key);
const int *oml_int(const section *s, const char *key);
const double *oml_double(const section *s, const char *key);
const bool *oml_bool(const section *s, const char *key);

void oml_free(table *t);

#endif