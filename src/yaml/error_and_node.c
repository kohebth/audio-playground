#include "error.h"
#include "node.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void uc_error_set(uc_error *e, uc_status s, uc_loc loc, const char *fmt, ...) {
    if (!e) return;
    e->status = s;
    e->loc    = loc;
    va_list ap; va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
}

const char *uc_status_str(uc_status s) {
    switch (s) {
        case UC_OK:        return "ok";
        case UC_E_IO:      return "io error";
        case UC_E_OOM:     return "out of memory";
        case UC_E_LEX:     return "lex error";
        case UC_E_PARSE:   return "parse error";
        case UC_E_TYPE:    return "type error";
        case UC_E_RANGE:   return "range error";
        case UC_E_MISSING: return "missing field";
    }
    return "?";
}

const uc_node *uc_node_find(const uc_node *n, const char *key) {
    if (!n || n->kind != UC_NODE_MAP) return NULL;
    for (size_t i = 0; i < n->map_len; ++i)
        if (strcmp(n->map[i].key, key) == 0) return n->map[i].value;
    return NULL;
}