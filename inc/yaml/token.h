#ifndef UNITCFG_TOKEN_H
#define UNITCFG_TOKEN_H

#include "error.h"

typedef enum {
    UC_TOK_INDENT,
    UC_TOK_NEWLINE,
    UC_TOK_DASH,
    UC_TOK_COLON,
    UC_TOK_LBRACE,
    UC_TOK_RBRACE,
    UC_TOK_COMMA,
    UC_TOK_SCALAR,
    UC_TOK_VARREF,
    UC_TOK_NULL,
    UC_TOK_EOF
} uc_tok_kind;

typedef struct {
    uc_tok_kind kind;
    const char *text;
    int         len;
    int         indent;
    uc_loc      loc;
} uc_token;

#endif