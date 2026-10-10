#include "liboml.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    for (char *e = s + strlen(s); e > s && isspace((unsigned char)e[-1]);) *--e = 0;
    return s;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *read_line(FILE *f, char **buf, size_t *cap)
{
    size_t len = 0;
    if (!*buf) {
        *cap = 128;
        if (!(*buf = malloc(*cap))) return NULL;
    }
    for (;;) {
        if (len + 2 > *cap) {
            size_t ncap = *cap * 2;
            char *p = realloc(*buf, ncap);
            if (!p) return NULL;
            *buf = p;
            *cap = ncap;
        }
        if (!fgets(*buf + len, (int)(*cap - len), f)) {
            if (len == 0) return NULL;
            break;
        }
        len += strlen(*buf + len);
        if (len > 0 && (*buf)[len - 1] == '\n') {
            (*buf)[--len] = 0;
            break;
        }
        if (feof(f)) break;
    }
    return *buf;
}

static int raw_append(char **buf, size_t *cap, size_t *len, const char *s, size_t slen)
{
    if (*len + slen + 1 > *cap) {
        size_t ncap = *cap ? *cap * 2 : 64;
        while (ncap < *len + slen + 1) ncap *= 2;
        char *p = realloc(*buf, ncap);
        if (!p) return 0;
        *buf = p;
        *cap = ncap;
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = 0;
    return 1;
}

void oml_free(table *t)
{
    if (!t) return;
    for (size_t i = 0; i < t->count; i++) {
        for (size_t j = 0; j < t->sections[i].count; j++) {
            entry *e = &t->sections[i].entries[j];
            free(e->key);
            free(e->value);
            if (e->array) {
                for (char **a = e->array; *a; a++) free(*a);
                free(e->array);
            }
        }
        free(t->sections[i].entries);
        free(t->sections[i].name);
    }
    free(t->sections);
    free(t);
}

table *oml_get(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;

    table *t = calloc(1, sizeof *t);
    char *linebuf = NULL, *pending = NULL, *freeline = NULL;
    size_t linecap = 0;
    char *rawbuf = NULL;
    size_t rawcap = 0, rawlen = 0;
    char *l, *s0, *k, *v, *e, *c;
    size_t cur = 0, len;
    int raw;

    if (!t) goto fail;
    if (!(t->sections = malloc(sizeof *t->sections))) goto fail;
    if (!(t->sections[0].name = xstrdup("main"))) goto fail;
    t->sections[0].entries = NULL;
    t->sections[0].count = 0;
    t->count = 1;

    for (;;) {
        freeline = NULL;
        if (pending) {
            l = pending;
            freeline = pending;
            pending = NULL;
        } else if (!(l = read_line(f, &linebuf, &linecap))) {
            break;
        }

        int skip_entry = 0;

        s0 = l;
        while (isspace((unsigned char)*s0)) s0++;
        e = strchr(s0, '=');
        v = e ? e + 1 : NULL;
        while (v && (*v == ' ' || *v == '\t')) v++;
        raw = *s0 != '#' && e && e > s0 && v && *v == '`';

        if (raw) {
            *e = 0;
            k = xstrdup(trim(s0));
            if (!k) goto fail;
            v++;
            if ((c = strchr(v, '`'))) {
                *c = 0;
                char *val = xstrdup(v);
                if (!val) {
                    free(k);
                    goto fail;
                }
                if (c[1]) {
                    pending = xstrdup(c + 1);
                    if (!pending) {
                        free(k);
                        free(val);
                        goto fail;
                    }
                }
                v = val;
            } else {
                rawlen = 0;
                if (!raw_append(&rawbuf, &rawcap, &rawlen, v, strlen(v)) ||
                    !raw_append(&rawbuf, &rawcap, &rawlen, "\n", 1)) {
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
                        if (!raw_append(&rawbuf, &rawcap, &rawlen, linebuf, strlen(linebuf))) {
                            free(k);
                            goto fail;
                        }
                        if (c[1]) {
                            pending = xstrdup(c + 1);
                            if (!pending) {
                                free(k);
                                goto fail;
                            }
                        }
                        break;
                    }
                    if (!raw_append(&rawbuf, &rawcap, &rawlen, linebuf, strlen(linebuf)) ||
                        !raw_append(&rawbuf, &rawcap, &rawlen, "\n", 1)) {
                        free(k);
                        goto fail;
                    }
                }
                v = xstrdup(rawbuf);
                if (!v) {
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
                if (!*l) goto fail;
                for (cur = 0; cur < t->count && strcmp(t->sections[cur].name, l); cur++) {}
                if (cur == t->count) {
                    section *p = realloc(t->sections, (cur + 1) * sizeof *p);
                    if (!p) goto fail;
                    t->sections = p;
                    if (!(t->sections[cur].name = xstrdup(l))) goto fail;
                    t->sections[cur].entries = NULL;
                    t->sections[cur].count = 0;
                    t->count++;
                }
                skip_entry = 1;
            } else if (!strcmp(l, "}")) {
                cur = 0;
                skip_entry = 1;
            } else {
                if (!(e = strchr(l, '=')) || e == l) goto fail;
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
            entry *p = realloc(s->entries, (s->count + 1) * sizeof *p);
            char **a = NULL, *str;
            size_t vl = strlen(v), cnt = 1;
            if (!p) {
                free(k);
                free(v);
                goto fail;
            }
            s->entries = p;
            if (!raw && vl >= 2 && v[0] == '[' && v[vl - 1] == ']') {
                v[--vl] = 0;
                char *inner = v + 1;
                for (c = inner; *c; c++) cnt += *c == ',';
                if (!(a = malloc((cnt + 1) * sizeof *a))) {
                    free(k);
                    free(v);
                    goto fail;
                }
                size_t ai = 0;
                for (str = inner;; str = c + 1) {
                    c = strchr(str, ',');
                    if (c) *c = 0;
                    char *tok = xstrdup(trim(str));
                    if (!tok) {
                        for (size_t z = 0; z < ai; z++) free(a[z]);
                        free(a);
                        free(k);
                        free(v);
                        goto fail;
                    }
                    a[ai++] = tok;
                    if (!c) break;
                }
                if (ai && !*a[ai - 1]) {
                    free(a[ai - 1]);
                    ai--;
                }
                a[ai] = NULL;
                free(v);
                v = NULL;
            }
            s->entries[s->count++] = (entry){ k, v, a };
        }

        free(freeline);
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

const entry *oml_bykey(const table *t, const char *section_name, const char *key)
{
    if (!t || !key) return NULL;
    for (size_t i = 0; i < t->count; i++)
        if (!strcmp(t->sections[i].name, section_name ? section_name : "main"))
            for (size_t j = 0; j < t->sections[i].count; j++)
                if (!strcmp(t->sections[i].entries[j].key, key))
                    return &t->sections[i].entries[j];
    return NULL;
}