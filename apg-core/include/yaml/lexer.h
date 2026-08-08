#ifndef UNITCFG_LEXER_H
#define UNITCFG_LEXER_H

#include "arena.h"
#include "error.h"
#include "token.h"

typedef struct {
    uc_token *data;
    size_t    len;
    size_t    cap;
} uc_token_vec;

uc_status uc_lex(const char *src, size_t src_len,
                 uc_arena *arena,
                 uc_token_vec *out,
                 uc_error *err);

#endif