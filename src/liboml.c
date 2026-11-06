#include "liboml.h"
#include <ctype.h>
#include <liburing.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#ifndef OML_QUIET
#define oml_error(line, message) \
    fprintf(stderr, "%s: %s on line %zu\n", \
            (filename), (message), (size_t)(line))
#else
#define oml_error(line, message) ((void)(message))
#endif
static char *trim(char *s) {
    while (isspace((unsigned char)*s))
        s++;
    for (char *e = s + strlen(s);
         e > s && isspace((unsigned char)e[-1]);)
        *--e = 0;
    return s;
}
static char *xstrdup(const char *s) {
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p)
        memcpy(p, s, n);
    return p;
}
static void *grow(void *arr, size_t *cap,
                  size_t elem_size, size_t need) {
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
#define OML_BATCH 8
#define OML_READ_CHUNK (64 * 1024)

struct input_file {
    FILE *stream;
    char *data;
    size_t size, capacity;
    bool done;
};

static void prefetch(struct io_uring *ring, bool *ready,
                     const char **paths, struct input_file *files, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        files[i].stream = fopen(paths[i], "r");
        struct stat st;
        files[i].done = !files[i].stream ||
            fstat(fileno(files[i].stream), &st) != 0 || !S_ISREG(st.st_mode);
    }
    if (!*ready)
        return;

    for (;;) {
        size_t pending = 0;
        for (size_t i = 0; i < count; i++) {
            struct input_file *f = &files[i];
            if (f->done)
                continue;
            if (f->size > SIZE_MAX - OML_READ_CHUNK - 1)
                goto fallback;
            char *data = grow(f->data, &f->capacity, 1,
                              f->size + OML_READ_CHUNK + 1);
            if (!data)
                goto fallback;
            f->data = data;
            struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
            if (!sqe)
                goto fallback;
            io_uring_prep_read(sqe, fileno(f->stream), data + f->size,
                               OML_READ_CHUNK, f->size);
            io_uring_sqe_set_data(sqe, f);
            pending++;
        }
        if (!pending)
            break;
        size_t submitted = 0;
        while (submitted < pending) {
            int n = io_uring_submit(ring);
            if (n == -EINTR)
                continue;
            if (n <= 0)
                goto fallback;
            submitted += (size_t)n;
        }
        while (pending--) {
            struct io_uring_cqe *cqe;
            int rc;
            do {
                rc = io_uring_wait_cqe(ring, &cqe);
            } while (rc == -EINTR);
            if (rc < 0)
                goto fallback;
            struct input_file *f = io_uring_cqe_get_data(cqe);
            int n = cqe->res;
            io_uring_cqe_seen(ring, cqe);
            if (n < 0)
                goto fallback;
            if (n == 0)
                f->done = true;
            else
                f->size += (size_t)n;
        }
    }
    for (size_t i = 0; i < count; i++) {
        struct input_file *f = &files[i];
        if (!f->data)
            continue;
        f->data[f->size] = 0;
        FILE *memory = fmemopen(f->data, f->size, "r");
        if (memory) {
            fclose(f->stream);
            f->stream = memory;
        }
    }
    return;

fallback:
    io_uring_queue_exit(ring);
    *ready = false;
    for (size_t i = 0; i < count; i++) {
        free(files[i].data);
        files[i].data = NULL;
    }
}

static char *read_line(FILE *f, char **buf, size_t *cap, int *status) {
    size_t len = 0;
    *status = -1;
    for (;;) {
        if (len > SIZE_MAX - 2)
            return NULL;
        char *p = grow(*buf, cap, 1, len + 2);
        if (!p)
            return NULL;
        *buf = p;
        if (!fgets(*buf + len, (int)((*cap - len > INT_MAX) ? INT_MAX : *cap - len), f)) {
            if (ferror(f) || !len) {
                *status = ferror(f) ? -2 : 0;
                return NULL;
            }
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
    *status = 1;
    return *buf;
}
static int raw_append(char **buf, size_t *cap,
                      size_t *len, const char *s, size_t slen) {
    if (slen > SIZE_MAX - *len - 1)
        return 0;
    char *p = grow(*buf, cap, 1, *len + slen + 1);
    if (!p)
        return 0;
    *buf = p;
    if (slen)
        memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = 0;
    return 1;
}
static entry *find_entry(const section *s, const char *key) {
    if (!s || !key || !s->entries)
        return NULL;
    for (size_t i = 0; i < s->size; i++)
        if (s->entries[i].key && !strcmp(s->entries[i].key, key))
            return &s->entries[i];
    return NULL;
}
static void free_entry(entry *e) {
    free(e->key);
    free(e->value);
    if (e->array) {
        for (char **a = e->array; *a; a++)
            free(*a);
        free(e->array);
    }
}
static size_t find_section(const table *t, size_t parent, const char *name) {
    for (size_t i = 0; i < t->size; i++) {
        const section *s = &t->sections[i];
        if (i != parent && s->parent == parent && s->name &&
            !strcmp(s->name, name))
            return i;
    }
    return t->size;
}
void oml_free(table *t) {
    if (!t)
        return;
    for (size_t i = 0; i < t->size; i++) {
        section *s = &t->sections[i];
        for (size_t j = 0; j < s->size; j++)
            free_entry(&s->entries[j]);
        free(s->entries);
        free(s->name);
    }
    free(t->sections);
    free(t);
}
static int parse_file(table *t, const char *filename,
                      size_t base, unsigned depth, bool replace,
                      struct io_uring *ring, bool *ready, FILE *input) {
    FILE *f = input ? input : fopen(filename, "r");
    if (!f)
        return errno == ENOENT ? -1 : 0;
    struct include_file {
        char *path;
        size_t section, line;
        bool replace;
    } *appends = NULL;
    size_t append_count = 0, append_capacity = 0;
    int ok = 0;
    entry staged = {0};
    char *linebuf = NULL, *pending = NULL, *freeline = NULL, *rawbuf = NULL;
    size_t linecap = 0, rawcap = 0, rawlen = 0, line_number = 0, raw_start = 0;
    const char *error_message = "out of memory";
    int read_status = 0;
    char *l, *s0, *v, *e, *c;
    size_t cur = base, len;
    int raw;
    for (;;) {
        freeline = NULL;
        if (pending) {
            l = freeline = pending;
            pending = NULL;
        } else {
            l = read_line(f, &linebuf, &linecap, &read_status);
            if (!l) {
                if (read_status == 0)
                    break;
                error_message = read_status == -2
                    ? "failed to read file" : "out of memory";
                goto fail;
            }
            line_number++;
        }
        int skip_entry = 0;
        s0 = l;
        while (isspace((unsigned char)*s0))
            s0++;
        bool override = !strncmp(s0, "override:", 9);
        if (override || !strncmp(s0, "append:", 7)) {
            if (depth >= 32) {
                error_message = override ? "override nesting limit exceeded"
                                         : "append nesting limit exceeded";
                goto fail;
            }
            const char *slash = strrchr(filename, '/');
            size_t dir_len = slash ? (size_t)(slash - filename) + 1 : 0;
            char *item = s0 + (override ? 9 : 7);
            char *comma;
            do {
                comma = strchr(item, ',');
                if (comma)
                    *comma = '\0';
                char *name = trim(item);
                if (!*name) {
                    error_message = override ? "missing filename in override"
                                             : "missing filename in append";
                    goto fail;
                }
                void *p = grow(appends, &append_capacity,
                               sizeof *appends, append_count + 1);
                if (!p)
                    goto fail;
                appends = p;
                size_t prefix = name[0] == '/' ? 0 : dir_len;
                size_t len = strlen(name) + 1;
                if (prefix > SIZE_MAX - len)
                    goto fail;
                char *path = malloc(prefix + len);
                if (!path)
                    goto fail;
                memcpy(path, filename, prefix);
                memcpy(path + prefix, name, len);
                appends[append_count++] =
                    (struct include_file){ path, cur, line_number, override };
                if (comma)
                    item = comma + 1;
            } while (comma);
            free(freeline);
            freeline = NULL;
            continue;
        }
        e = strchr(s0, '=');
        v = e ? e + 1 : NULL;
        while (v && (*v == ' ' || *v == '\t'))
            v++;
        raw = *s0 != '#' && e && e > s0 && v && *v == '`';
        if (raw) {
            raw_start = line_number;
            *e = 0;
            if (!(staged.key = xstrdup(trim(s0))))
                goto fail;
            char *part = v + 1;
            rawlen = 0;
            while (!(c = strchr(part, '`'))) {
                if (!raw_append(&rawbuf, &rawcap, &rawlen,
                                part, strlen(part)) ||
                    !raw_append(&rawbuf, &rawcap, &rawlen, "\n", 1))
                    goto fail;
                part = read_line(f, &linebuf, &linecap, &read_status);
                if (!part) {
                    if (read_status == 0) {
                        error_message = "unclosed multiline value";
                        line_number = raw_start;
                    } else {
                        error_message = read_status == -2
                            ? "failed to read file" : "out of memory";
                    }
                    goto fail;
                }
                line_number++;
            }
            *c = 0;
            if (rawlen && !raw_append(&rawbuf, &rawcap, &rawlen,
                                      part, (size_t)(c - part)))
                goto fail;
            staged.value = xstrdup(rawlen ? rawbuf : part);
            if (!staged.value || (c[1] && !(pending = xstrdup(c + 1))))
                goto fail;
        } else {
            l = trim(l);
            len = strlen(l);
            if (!len || *l == '#') {
                skip_entry = 1;
            } else if (l[len - 1] == '{') {
                l[len - 1] = 0;
                l = trim(l);
                if (!*l) {
                    error_message = "missing section name";
                    goto fail;
                }
                size_t parent = cur;
                size_t j = find_section(t, parent, l);
                if (j < t->size && !replace) {
                    error_message = "duplicate section";
                    goto fail;
                }
                if (j == t->size) {
                    section *p = grow(
                        t->sections, &t->capacity,
                        sizeof *t->sections, t->size + 1);
                    if (!p)
                        goto fail;
                    t->sections = p;
                    t->sections[j] = (section){ .parent = parent };
                    t->size++;
                    if (!(t->sections[j].name = xstrdup(l)))
                        goto fail;
                }
                cur = j;
                skip_entry = 1;
            } else if (!strcmp(l, "}")) {
                if (cur == base) {
                    error_message = "unexpected closing brace";
                    goto fail;
                }
                cur = t->sections[cur].parent;
                skip_entry = 1;
            } else {
                if (!(e = strchr(l, '=')) || e == l) {
                    error_message = "invalid syntax";
                    goto fail;
                }
                *e = 0;
                staged.key = xstrdup(trim(l));
                staged.value = xstrdup(trim(e + 1));
                if (!staged.key || !staged.value)
                    goto fail;
            }
        }
        if (!skip_entry) {
            section *s = &t->sections[cur];
            entry *existing = find_entry(s, staged.key);
            if (existing && !replace) {
                error_message = "duplicate key";
                if (raw)
                    line_number = raw_start;
                goto fail;
            }
            if (!existing) {
                entry *p = grow(s->entries, &s->capacity,
                                sizeof *s->entries, s->size + 1);
                if (!p)
                    goto fail;
                s->entries = p;
            }
            char *str;
            size_t vl = strlen(staged.value), cnt = 1;
            if (!raw && vl >= 2 &&
                staged.value[0] == '[' && staged.value[vl - 1] == ']') {
                staged.value[--vl] = 0;
                char *inner = staged.value + 1;
                for (c = inner; *c; c++)
                    cnt += *c == ',';
                if (cnt > SIZE_MAX / sizeof *staged.array - 1)
                    goto fail;
                staged.array = calloc(cnt + 1, sizeof *staged.array);
                if (!staged.array)
                    goto fail;
                size_t ai = 0;
                if (*trim(inner)) {
                    for (str = inner;; str = c + 1) {
                        c = strchr(str, ',');
                        if (c)
                            *c = 0;
                        char *tok = xstrdup(trim(str));
                        if (!tok)
                            goto fail;
                        staged.array[ai++] = tok;
                        if (!c)
                            break;
                    }
                    if (ai && !*staged.array[ai - 1])
                        free(staged.array[--ai]);
                }
                staged.array[ai] = NULL;
                free(staged.value);
                staged.value = NULL;
            }
            if (existing)
                free_entry(existing);
            else
                existing = &s->entries[s->size++];
            *existing = staged;
            staged = (entry){0};
        }
        free(freeline);
        freeline = NULL;
    }
    if (cur != base) {
        error_message = "unclosed section";
        goto fail;
    }
    for (size_t start = 0; start < append_count; start += OML_BATCH) {
        size_t count = append_count - start;
        if (count > OML_BATCH)
            count = OML_BATCH;
        struct input_file files[OML_BATCH] = {0};
        const char *paths[OML_BATCH];
        for (size_t n = 0; n < count; n++)
            paths[n] = appends[start + n].path;
        prefetch(ring, ready, paths, files, count);
        bool batch_ok = true;
        for (size_t n = 0; n < count; n++) {
            size_t i = start + n;
            int result = parse_file(t, appends[i].path,
                                    appends[i].section, depth + 1,
                                    appends[i].replace, ring, ready, files[n].stream);
            files[n].stream = NULL;
            free(files[n].data);
            files[n].data = NULL;
            if (result == -1) {
#ifndef OML_QUIET
                fprintf(stderr, "%s: file %s not found!\n",
                        filename, appends[i].path);
#endif
            } else if (!result) {
                line_number = appends[i].line;
                error_message = appends[i].replace ? "failed to override from file"
                                                   : "failed to append file";
                batch_ok = false;
                break;
            }
        }
        for (size_t n = 0; n < count; n++) {
            if (files[n].stream)
                fclose(files[n].stream);
            free(files[n].data);
        }
        if (!batch_ok)
            goto fail;
    }
    ok = 1;
    goto cleanup;
fail:
    oml_error(line_number, error_message);
cleanup:
    free_entry(&staged);
    for (size_t i = 0; i < append_count; i++)
        free(appends[i].path);
    free(appends);
    free(linebuf);
    free(rawbuf);
    free(pending);
    free(freeline);
    fclose(f);
    return ok;
}
table *oml_get(const char *filename) {
    if (!filename)
        return NULL;
    table *t = calloc(1, sizeof *t);
    if (!t)
        return NULL;
    t->sections = grow(NULL, &t->capacity, sizeof *t->sections, 1);
    if (t->sections) {
        t->sections[0] = (section){0};
        t->size = 1;
        t->sections[0].name = xstrdup("__undefined");
        if (t->sections[0].name) {
            struct io_uring ring;
            bool ready = io_uring_queue_init(OML_BATCH, &ring, 0) == 0;
            struct input_file input = {0};
            prefetch(&ring, &ready, &filename, &input, 1);
            int result = parse_file(t, filename, 0, 0, false,
                                    &ring, &ready, input.stream);
            free(input.data);
            if (ready)
                io_uring_queue_exit(&ring);
            if (result == 1)
                return t;
        }
    }
    oml_free(t);
    return NULL;
}
const section *oml_section(table *t, const char *name) {
    if (!t || !t->sections || !t->size)
        return NULL;
    if (!name || !strcmp(name, "__undefined"))
        return &t->sections[0];
    size_t i = find_section(t, 0, name);
    return i < t->size ? &t->sections[i] : NULL;
}
const section *oml_nested(table *t, const section *parent,
                          const char *name) {
    if (!t || !t->sections || !t->size || !parent || !name)
        return NULL;
    size_t pidx;
    for (pidx = 0; pidx < t->size; pidx++)
        if (&t->sections[pidx] == parent)
            break;
    if (pidx == t->size)
        return NULL;
    size_t i = find_section(t, pidx, name);
    return i < t->size ? &t->sections[i] : NULL;
}
const entry *oml_value(const section *s, const char *key) {
    const entry *e = find_entry(s, key);
    if (e && e->value) {
        char *v = e->value;
        size_t len = strlen(v);
        if (len >= 2 && v[0] == '"' && v[len - 1] == '"') {
            memmove(v, v + 1, len - 2);
            v[len - 2] = '\0';
        }
    }
    return e;
}
const int *oml_int(const section *s, const char *key) {
    const entry *e = oml_value(s, key);
    if (!e || !e->value)
        return NULL;
    char *end;
    errno = 0;
    long value = strtol(e->value, &end, 10);
    if (end == e->value || *end != '\0' || errno == ERANGE ||
        value < INT_MIN || value > INT_MAX)
        return NULL;
    int *result = malloc(sizeof *result);
    if (result)
        *result = (int)value;
    return result;
}
const double *oml_double(const section *s, const char *key) {
    const entry *e = oml_value(s, key);
    if (!e || !e->value)
        return NULL;
    char *end;
    errno = 0;
    double value = strtod(e->value, &end);
    if (end == e->value || *end != '\0' || errno == ERANGE ||
        !isfinite(value))
        return NULL;
    double *result = malloc(sizeof *result);
    if (result)
        *result = value;
    return result;
}
const bool *oml_bool(const section *s, const char *key) {
    const entry *e = oml_value(s, key);
    if (!e || !e->value)
        return NULL;
    bool value;
    if (strcmp(e->value, "true") == 0)
        value = true;
    else if (strcmp(e->value, "false") == 0)
        value = false;
    else
        return NULL;
    bool *result = malloc(sizeof *result);
    if (result)
        *result = value;
    return result;
}