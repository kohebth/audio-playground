#ifndef UNITCFG_LOADER_H
#define UNITCFG_LOADER_H

#include "arena.h"
#include "error.h"
#include "unit.h"

uc_status uc_load_string(const char *src, size_t src_len,
                         uc_arena *arena, uc_unit *out, uc_error *err);

uc_status uc_load_file  (const char *path,
                         uc_arena *arena, uc_unit *out, uc_error *err);

#endif