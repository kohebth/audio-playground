#ifndef UNITCFG_LOADER_H
#define UNITCFG_LOADER_H

#include "arena.h"
#include "error.h"
#include "unit.h"

#if defined(APG_ENABLE_V1_DEPRECATED_WARNINGS)
#if defined(__GNUC__) || defined(__clang__)
#define APG_YAML_V1_DEPRECATED \
    __attribute__((deprecated("uc_load_* is the v1 unit loader; use APGCore v2 parser/validator APIs")))
#else
#define APG_YAML_V1_DEPRECATED
#endif
#else
#define APG_YAML_V1_DEPRECATED
#endif

// Legacy v1 unit YAML loader. The low-level lexer/parser remains shared by APGCore v2.
APG_YAML_V1_DEPRECATED uc_status
uc_load_string(const char *src, size_t src_len, uc_arena *arena, uc_unit *out, uc_error *err);

APG_YAML_V1_DEPRECATED uc_status uc_load_file(const char *path, uc_arena *arena, uc_unit *out, uc_error *err);

#endif
