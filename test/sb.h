#ifndef SB_H
#define SB_H

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

struct sb {
    int count;
    int capacity;
    char * data;
};

static void * sb_oom(void * p) {
    if (!p) { printf("Out of Memory\n"); exit(1); }
    return p;
}

static void sb_grow(struct sb * b, int extra) {
    int needed = b->count + extra + 1;
    if (needed > b->capacity) {
        b->capacity = needed * 2;
        b->data = (char *)sb_oom(realloc(b->data, b->capacity));
    }
}

static void sb_put(struct sb * b, const char * d, int bytes) {
    sb_grow(b, bytes);
    memcpy(b->data + b->count, d, bytes);
    b->count = b->count + bytes;
    b->data[b->count] = '\0';
}

static void sb_puts(struct sb * b, const char * s) {
    sb_put(b, s, (int)strlen(s));
}

static void sb_putc(struct sb * b, char c) { sb_put(b, &c, 1); }

#ifndef __cx__
#error This should NOT be seen
static void sb_printf(struct sb * b, const char * f, ...) {
    va_list ap;
    va_start(ap, f);
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(0, 0, f, copy);
    va_end(copy);
    if (n > 0) {
        sb_grow(b, n);
        vsnprintf(b->data + b->count, n + 1, f, ap);
        b->count = b->count + n;
    }
    va_end(ap);
}
#endif

static void sb_free(struct sb * b) {
    free(b->data);
    b->data = 0;
    b->count = 0;
    b->capacity = 0;
}

#endif
