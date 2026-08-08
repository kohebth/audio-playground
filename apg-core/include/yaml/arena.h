#ifndef UNITCFG_ARENA_H
#define UNITCFG_ARENA_H

#include <stddef.h>

typedef struct uc_arena {
    unsigned char *base;
    size_t         size;
    size_t         used;
    int            owns_buffer;
} uc_arena;

int   uc_arena_init      (uc_arena *a, size_t bytes);
void  uc_arena_init_fixed(uc_arena *a, void *buf, size_t bytes);
void  uc_arena_free      (uc_arena *a);
void  uc_arena_reset     (uc_arena *a);
void *uc_arena_alloc     (uc_arena *a, size_t bytes, size_t align);
char *uc_arena_strndup   (uc_arena *a, const char *s, size_t n);

#endif