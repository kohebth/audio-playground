#ifndef UNITCFG_PARSER_H
#define UNITCFG_PARSER_H

#include "arena.h"
#include "lexer.h"
#include "node.h"

uc_status uc_parse(const uc_token_vec *tokens,
                    uc_arena *arena,
                    uc_node **out_root,
                    uc_error *err);

#endif