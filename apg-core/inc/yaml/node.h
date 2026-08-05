#ifndef UNITCFG_NODE_H
#define UNITCFG_NODE_H

#include <stddef.h>

typedef enum {
    UC_NODE_NULL,
    UC_NODE_SCALAR,
    UC_NODE_VARREF,
    UC_NODE_MAP,
    UC_NODE_SEQ
} uc_node_kind;

typedef struct uc_node uc_node;

typedef struct {
    const char *key;
    uc_node    *value;
} uc_pair;

struct uc_node {
    uc_node_kind kind;

    const char *text;
    int         text_len;

    uc_pair *map;
    size_t   map_len;
    size_t   map_cap;

    uc_node **seq;
    size_t    seq_len;
    size_t    seq_cap;
};

const uc_node *uc_node_find(const uc_node *n, const char *key);

#endif