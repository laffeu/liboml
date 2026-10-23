#include "liboml.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;

    for (char *e = s + strlen(s);
         e > s && isspace((unsigned char)e[-1]);)
        *--e = 0;

    return s;
}

static char *xstrdup(const char *s)
{
    if (!s)
        return NULL;

    size_t n = strlen(s) + 1;
    char *p = malloc(n);

    if (p)
        memcpy(p, s, n);

    return p;
}

static void *grow(void *arr, size_t *cap,
                  size_t elem_size, size_t need)
{
    if (need <= *cap)
        return arr;

    if (!elem_size || need > SIZE_MAX / elem_size)
        return NULL;

    size_t ncap = *cap ? *cap : 4;

    while (ncap < need) {
        if (ncap > SIZE_MAX / 2) {
            ncap = need;
            break;
        }

        ncap *= 2;
    }

    if (ncap > SIZE_MAX / elem_size)
        return NULL;

    void *p = realloc(arr, ncap * elem_size);

    if (!p)
        return NULL;

    *cap = ncap;
    return p;
}

static char *read_line(FILE *f, char **buf, size_t *cap)
{
    size_t len = 0;

    if (!*buf) {
        *cap = 128;
        *buf = malloc(*cap);

        if (!*buf)
            return NULL;
    }

    for (;;) {
        if (len + 2 > *cap) {
            if (*cap > SIZE_MAX / 2)
                return NULL;

            size_t ncap = *cap * 2;
            char *p = realloc(*buf, ncap);

            if (!p)
                return NULL;

            *buf = p;
            *cap = ncap;
        }

        if (!fgets(*buf + len, (int)(*cap - len), f)) {
            if (ferror(f))
                return NULL;

            if (!len)
                return NULL;

            break;
        }

        len += strlen(*buf + len);

        if (len && (*buf)[len - 1] == '\n') {
            (*buf)[--len] = 0;
            break;
        }

        if (feof(f))
            break;
    }

    return *buf;
}

static int raw_append(char **buf, size_t *cap,
                      size_t *len, const char *s, size_t slen)
{
    if (slen > SIZE_MAX - *len - 1)
        return 0;

    size_t need = *len + slen + 1;

    if (need > *cap) {
        size_t ncap = *cap ? *cap : 64;

        while (ncap < need) {
            if (ncap > SIZE_MAX / 2) {
                ncap = need;
                break;
            }

            ncap *= 2;
        }

        char *p = realloc(*buf, ncap);

        if (!p)
            return 0;

        *buf = p;
        *cap = ncap;
    }

    if (slen)
        memcpy(*buf + *len, s, slen);

    *len += slen;
    (*buf)[*len] = 0;

    return 1;
}

void oml_free(table *t)
{
    if (!t)
        return;

    for (size_t i = 0; i < t->size; i++) {
        section *s = &t->sections[i];

        for (size_t j = 0; j < s->size; j++) {
            entry *e = &s->entries[j];

            free(e->key);
            free(e->value);

            if (e->array) {
                for (char **a = e->array; *a; a++)
                    free(*a);

                free(e->array);
            }
        }

        free(s->entries);
        free(s->name);
    }

    free(t->sections);
    free(t);
}

table *oml_get(const char *filename)
{
    if (!filename)
        return NULL;

    FILE *f = fopen(filename, "r");

    if (!f)
        return NULL;

    table *t = calloc(1, sizeof *t);

    char *linebuf = NULL;
    char *pending = NULL;
    char *freeline = NULL;
    char *rawbuf = NULL;

    size_t linecap = 0;
    size_t rawcap = 0;
    size_t rawlen = 0;

    char *l, *s0, *k, *v, *e, *c;
    size_t cur = 0, len;
    int raw;

    if (!t)
        goto fail;

    t->sections = grow(NULL, &t->capacity,
                       sizeof *t->sections, 1);

    if (!t->sections)
        goto fail;

    t->sections[0] = (section){0};

    if (!(t->sections[0].name = xstrdup("__undefined")))
        goto fail;

    t->size = 1;

    for (;;) {
        freeline = NULL;

        if (pending) {
            l = freeline = pending;
            pending = NULL;
        } else {
            l = read_line(f, &linebuf, &linecap);

            if (!l) {
                if (ferror(f))
                    goto fail;

                break;
            }
        }

        int skip_entry = 0;

        s0 = l;

        while (isspace((unsigned char)*s0))
            s0++;

        e = strchr(s0, '=');
        v = e ? e + 1 : NULL;

        while (v && (*v == ' ' || *v == '\t'))
            v++;

        raw = *s0 != '#' && e && e > s0 && v && *v == '`';

        if (raw) {
            *e = 0;

            if (!(k = xstrdup(trim(s0))))
                goto fail;

            v++;

            if ((c = strchr(v, '`'))) {
                *c = 0;

                char *val = xstrdup(v);

                if (!val) {
                    free(k);
                    goto fail;
                }

                if (c[1] && !(pending = xstrdup(c + 1))) {
                    free(k);
                    free(val);
                    goto fail;
                }

                v = val;
            } else {
                rawlen = 0;

                if (!raw_append(&rawbuf, &rawcap, &rawlen,
                                v, strlen(v)) ||
                    !raw_append(&rawbuf, &rawcap, &rawlen,
                                "\n", 1)) {
                    free(k);
                    goto fail;
                }

                for (;;) {
                    if (!read_line(f, &linebuf, &linecap)) {
                        free(k);
                        goto fail;
                    }

                    if ((c = strchr(linebuf, '`'))) {
                        *c = 0;

                        if (!raw_append(&rawbuf, &rawcap, &rawlen,
                                        linebuf, strlen(linebuf))) {
                            free(k);
                            goto fail;
                        }

                        if (c[1] && !(pending = xstrdup(c + 1))) {
                            free(k);
                            goto fail;
                        }

                        break;
                    }

                    if (!raw_append(&rawbuf, &rawcap, &rawlen,
                                    linebuf, strlen(linebuf)) ||
                        !raw_append(&rawbuf, &rawcap, &rawlen,
                                    "\n", 1)) {
                        free(k);
                        goto fail;
                    }
                }

                if (!(v = xstrdup(rawbuf))) {
                    free(k);
                    goto fail;
                }
            }
        } else {
            l = trim(l);
            len = strlen(l);

            if (!len || *l == '#') {
                skip_entry = 1;
            } else if (l[len - 1] == '{') {
                l[len - 1] = 0;
                l = trim(l);

                if (!*l)
                    goto fail;

                size_t parent = cur;
                size_t j;

                for (j = 0; j < t->size; j++) {
                    if (j != parent &&
                        t->sections[j].parent == parent &&
                        !strcmp(t->sections[j].name, l))
                        break;
                }

                if (j == t->size) {
                    section *p = grow(
                        t->sections, &t->capacity,
                        sizeof *t->sections, t->size + 1);

                    if (!p)
                        goto fail;

                    t->sections = p;
                    t->sections[j] = (section){0};
                    t->sections[j].parent = parent;

                    if (!(t->sections[j].name = xstrdup(l)))
                        goto fail;

                    t->size++;
                }

                cur = j;
                skip_entry = 1;
            } else if (!strcmp(l, "}")) {
                if (cur == 0)
                    goto fail;
                cur = t->sections[cur].parent;
                skip_entry = 1;
            } else {
                if (!(e = strchr(l, '=')) || e == l)
                    goto fail;

                *e = 0;

                k = xstrdup(trim(l));
                v = xstrdup(trim(e + 1));

                if (!k || !v) {
                    free(k);
                    free(v);
                    goto fail;
                }
            }
        }

        if (!skip_entry) {
            section *s = &t->sections[cur];

            entry *p = grow(
                s->entries, &s->capacity,
                sizeof *s->entries, s->size + 1);

            char **a = NULL;
            char *str;

            if (!p) {
                free(k);
                free(v);
                goto fail;
            }

            s->entries = p;

            size_t vl = strlen(v);
            size_t cnt = 1;

            if (!raw && vl >= 2 &&
                v[0] == '[' && v[vl - 1] == ']') {
                v[--vl] = 0;

                char *inner = v + 1;

                for (c = inner; *c; c++)
                    cnt += *c == ',';

                if (cnt > SIZE_MAX / sizeof *a - 1) {
                    free(k);
                    free(v);
                    goto fail;
                }

                a = malloc((cnt + 1) * sizeof *a);

                if (!a) {
                    free(k);
                    free(v);
                    goto fail;
                }

                size_t ai = 0;

                if (*trim(inner)) {
                    for (str = inner;; str = c + 1) {
                        c = strchr(str, ',');

                        if (c)
                            *c = 0;

                        char *tok = xstrdup(trim(str));

                        if (!tok) {
                            for (size_t z = 0; z < ai; z++)
                                free(a[z]);

                            free(a);
                            free(k);
                            free(v);
                            goto fail;
                        }

                        a[ai++] = tok;

                        if (!c)
                            break;
                    }

                    if (ai && !*a[ai - 1])
                        free(a[--ai]);
                }

                a[ai] = NULL;

                free(v);
                v = NULL;
            }

            s->entries[s->size++] = (entry){ k, v, a };
        }

        free(freeline);
        freeline = NULL;
    }

    free(linebuf);
    free(rawbuf);
    free(pending);
    fclose(f);

    return t;

fail:
    free(linebuf);
    free(rawbuf);
    free(pending);
    free(freeline);
    fclose(f);
    oml_free(t);

    return NULL;
}

const section *oml_section(const table *t, const char *name)
{
    if (!t || !t->sections || !t->size)
        return NULL;

    if (!name || !strcmp(name, "__undefined"))
        return &t->sections[0];

    for (size_t i = 1; i < t->size; i++) {
        if (t->sections[i].parent == 0 &&
            t->sections[i].name &&
            !strcmp(t->sections[i].name, name))
            return &t->sections[i];
    }

    return NULL;
}

const section *oml_nested(const table *t, const section *parent,
                          const char *name)
{
    if (!t || !t->sections || !t->size || !parent || !name)
        return NULL;

    size_t pidx;

    for (pidx = 0; pidx < t->size; pidx++) {
        if (&t->sections[pidx] == parent)
            break;
    }

    if (pidx == t->size)
        return NULL;

    for (size_t i = 0; i < t->size; i++) {
        if (i != pidx &&
            t->sections[i].parent == pidx &&
            t->sections[i].name &&
            !strcmp(t->sections[i].name, name))
            return &t->sections[i];
    }

    return NULL;
}

const entry *oml_key(const section *s, const char *key)
{
    if (!s || !key || !s->entries)
        return NULL;

    for (size_t i = 0; i < s->size; i++) {
        if (s->entries[i].key &&
            !strcmp(s->entries[i].key, key))
            return &s->entries[i];
    }

    return NULL;
}