#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int uc_arena_init(uc_arena *a, size_t bytes) {
    a->base = (unsigned char *)malloc(bytes);
    if (!a->base) return -1;
    a->size = bytes;
    a->used = 0;
    a->owns_buffer = 1;
    return 0;
}

void uc_arena_init_fixed(uc_arena *a, void *buf, size_t bytes) {
    a->base = (unsigned char *)buf;
    a->size = bytes;
    a->used = 0;
    a->owns_buffer = 0;
}

void uc_arena_free(uc_arena *a) {
    if (a->owns_buffer && a->base) free(a->base);
    a->base = NULL; a->size = a->used = 0; a->owns_buffer = 0;
}

void uc_arena_reset(uc_arena *a) { a->used = 0; }

void *uc_arena_alloc(uc_arena *a, size_t bytes, size_t align) {
    if (align == 0) align = sizeof(void *);
    uintptr_t p   = (uintptr_t)(a->base + a->used);
    uintptr_t pad = (align - (p & (align - 1))) & (align - 1);
    if (a->used + pad + bytes > a->size) return NULL;
    void *out = a->base + a->used + pad;
    a->used += pad + bytes;
    memset(out, 0, bytes);
    return out;
}

char *uc_arena_strndup(uc_arena *a, const char *s, size_t n) {
    char *p = (char *)uc_arena_alloc(a, n + 1, 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}