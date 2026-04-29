#include "loader.h"
#include "lexer.h"
#include "parser.h"
#include "node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uc_status to_double(const uc_node *n, const char *ctx,
                           double *out, uc_error *err) {
    if (!n || n->kind != UC_NODE_SCALAR) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_TYPE, l, "%s: expected number", ctx);
        return UC_E_TYPE;
    }
    char *end = NULL;
    double v = strtod(n->text, &end);
    if (end == n->text || (end && *end != '\0')) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_TYPE, l, "%s: invalid number '%s'", ctx, n->text);
        return UC_E_TYPE;
    }
    *out = v;
    return UC_OK;
}

static uc_status to_int(const uc_node *n, const char *ctx,
                        int *out, uc_error *err) {
    double d;
    uc_status s = to_double(n, ctx, &d, err);
    if (s != UC_OK) return s;
    *out = (int)d;
    return UC_OK;
}

static uc_value to_value(const uc_node *n) {
    uc_value v = { UC_VAL_LITERAL, "" };
    if (!n) return v;
    if (n->kind == UC_NODE_VARREF) { v.kind = UC_VAL_VARREF;  v.text = n->text; }
    else if (n->kind == UC_NODE_SCALAR) { v.kind = UC_VAL_LITERAL; v.text = n->text; }
    return v;
}

static uc_status fill_kv(const uc_node *m, uc_arena *a,
                         uc_kv **out, size_t *out_len, uc_error *err) {
    if (!m || m->kind != UC_NODE_MAP) { *out = NULL; *out_len = 0; return UC_OK; }
    uc_kv *arr = (uc_kv *)uc_arena_alloc(a, m->map_len * sizeof *arr, sizeof(void*));
    if (!arr && m->map_len) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_OOM, l, "arena OOM");
        return UC_E_OOM;
    }
    for (size_t i = 0; i < m->map_len; ++i) {
        arr[i].key   = m->map[i].key;
        arr[i].value = to_value(m->map[i].value);
    }
    *out = arr; *out_len = m->map_len;
    return UC_OK;
}

static uc_status fill_kv_d(const uc_node *m, uc_arena *a, const char *ctx,
                           uc_kv_d **out, size_t *out_len, uc_error *err) {
    if (!m || m->kind != UC_NODE_MAP) { *out = NULL; *out_len = 0; return UC_OK; }
    uc_kv_d *arr = (uc_kv_d *)uc_arena_alloc(a, m->map_len * sizeof *arr, sizeof(void*));
    if (!arr && m->map_len) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_OOM, l, "arena OOM");
        return UC_E_OOM;
    }
    for (size_t i = 0; i < m->map_len; ++i) {
        arr[i].key = m->map[i].key;
        uc_status s = to_double(m->map[i].value, ctx, &arr[i].value, err);
        if (s != UC_OK) return s;
    }
    *out = arr; *out_len = m->map_len;
    return UC_OK;
}

static uc_status build_param(const uc_node *n, uc_param *p, uc_error *err) {
    if (!n || n->kind != UC_NODE_MAP) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_TYPE, l, "param entry must be a mapping");
        return UC_E_TYPE;
    }
    const uc_node *x;
    if ((x = uc_node_find(n, "min"))     != NULL) { uc_status s = to_double(x, "min",     &p->min, err); if (s) return s; }
    if ((x = uc_node_find(n, "max"))     != NULL) { uc_status s = to_double(x, "max",     &p->max, err); if (s) return s; }
    if ((x = uc_node_find(n, "default")) != NULL) { uc_status s = to_double(x, "default", &p->def, err); if (s) return s; }
    if ((x = uc_node_find(n, "unit"))    != NULL && x->kind == UC_NODE_SCALAR) p->unit = x->text;
    return UC_OK;
}

static uc_status build_stage(const uc_node *n, uc_arena *a,
                             uc_stage *st, uc_error *err) {
    if (!n || n->kind != UC_NODE_MAP) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_TYPE, l, "pipeline entry must be a mapping");
        return UC_E_TYPE;
    }
    const uc_node *x;
    if ((x = uc_node_find(n, "id")) && x->kind == UC_NODE_SCALAR) st->id = x->text;
    if ((x = uc_node_find(n, "fn")) && x->kind == UC_NODE_SCALAR) st->fn = x->text;

    uc_status s;
    if ((s = fill_kv  (uc_node_find(n, "in"),     a,           &st->in,     &st->in_len,     err)) != UC_OK) return s;
    if ((s = fill_kv  (uc_node_find(n, "out"),    a,           &st->out,    &st->out_len,    err)) != UC_OK) return s;
    if ((s = fill_kv  (uc_node_find(n, "config"), a,           &st->config, &st->config_len, err)) != UC_OK) return s;
    if ((s = fill_kv_d(uc_node_find(n, "state"),  a, "state",  &st->state,  &st->state_len,  err)) != UC_OK) return s;
    return UC_OK;
}

static uc_status build_unit(const uc_node *root, uc_arena *a,
                           uc_unit *u, uc_error *err) {
    if (!root || root->kind != UC_NODE_MAP) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_TYPE, l, "root must be a mapping");
        return UC_E_TYPE;
    }

    const uc_node *x;
    if ((x = uc_node_find(root, "name"))    && x->kind == UC_NODE_SCALAR) u->name    = x->text;
    if ((x = uc_node_find(root, "version")) && x->kind == UC_NODE_SCALAR) u->version = x->text;

    if ((x = uc_node_find(root, "system")) && x->kind == UC_NODE_MAP) {
        const uc_node *y;
        if ((y = uc_node_find(x, "sample_rate")) != NULL) {
            uc_status s = to_int(y, "system.sample_rate", &u->system.sample_rate, err);
            if (s != UC_OK) return s;
        }
        if ((y = uc_node_find(x, "channel")) != NULL) {
            uc_status s = to_int(y, "system.channel", &u->system.channel, err);
            if (s != UC_OK) return s;
        }
    }

    if ((x = uc_node_find(root, "params")) && x->kind == UC_NODE_MAP) {
        u->params = (uc_param *)uc_arena_alloc(a, x->map_len * sizeof *u->params, sizeof(void*));
        if (!u->params && x->map_len) { uc_loc l={0,0}; uc_error_set(err,UC_E_OOM,l,"arena OOM"); return UC_E_OOM; }
        u->params_len = x->map_len;
        for (size_t i = 0; i < x->map_len; ++i) {
            u->params[i].name = x->map[i].key;
            u->params[i].unit = "";
            uc_status s = build_param(x->map[i].value, &u->params[i], err);
            if (s != UC_OK) return s;
        }
    }

    if ((x = uc_node_find(root, "internal")) && x->kind == UC_NODE_MAP) {
        uc_status s = fill_kv_d(x, a, "internal", &u->internal, &u->internal_len, err);
        if (s != UC_OK) return s;
    }

    if ((x = uc_node_find(root, "signals")) && x->kind == UC_NODE_SEQ) {
        u->signals = (const char **)uc_arena_alloc(a, x->seq_len * sizeof *u->signals, sizeof(void*));
        if (!u->signals && x->seq_len) { uc_loc l={0,0}; uc_error_set(err,UC_E_OOM,l,"arena OOM"); return UC_E_OOM; }
        u->signals_len = 0;
        for (size_t i = 0; i < x->seq_len; ++i) {
            const uc_node *it = x->seq[i];
            if (it->kind == UC_NODE_MAP) {
                const uc_node *nm = uc_node_find(it, "name");
                if (nm && nm->kind == UC_NODE_SCALAR)
                    u->signals[u->signals_len++] = nm->text;
            } else if (it->kind == UC_NODE_SCALAR) {
                u->signals[u->signals_len++] = it->text;
            }
        }
    }

    if ((x = uc_node_find(root, "pipeline")) && x->kind == UC_NODE_SEQ) {
        u->pipeline = (uc_stage *)uc_arena_alloc(a, x->seq_len * sizeof *u->pipeline, sizeof(void*));
        if (!u->pipeline && x->seq_len) { uc_loc l={0,0}; uc_error_set(err,UC_E_OOM,l,"arena OOM"); return UC_E_OOM; }
        u->pipeline_len = x->seq_len;
        for (size_t i = 0; i < x->seq_len; ++i) {
            uc_status s = build_stage(x->seq[i], a, &u->pipeline[i], err);
            if (s != UC_OK) return s;
        }
    }
    return UC_OK;
}

uc_status uc_load_string(const char *src, size_t src_len,
                         uc_arena *arena, uc_unit *out, uc_error *err) {
    memset(out, 0, sizeof *out);
    err->status = UC_OK;

    uc_token_vec toks = {0};
    uc_status s = uc_lex(src, src_len, arena, &toks, err);
    if (s != UC_OK) return s;

    uc_node *root = NULL;
    s = uc_parse(&toks, arena, &root, err);
    if (s != UC_OK) return s;

    return build_unit(root, arena, out, err);
}

uc_status uc_load_file(const char *path, uc_arena *arena,
                       uc_unit *out, uc_error *err) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        uc_loc l = {0,0};
        uc_error_set(err, UC_E_IO, l, "cannot open '%s'", path);
        return UC_E_IO;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); uc_loc l={0,0}; uc_error_set(err,UC_E_IO,l,"ftell"); return UC_E_IO; }

    char *buf = (char *)uc_arena_alloc(arena, (size_t)sz + 1, 1);
    if (!buf) { fclose(f); uc_loc l={0,0}; uc_error_set(err,UC_E_OOM,l,"arena OOM"); return UC_E_OOM; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); uc_loc l={0,0}; uc_error_set(err,UC_E_IO,l,"fread"); return UC_E_IO;
    }
    buf[sz] = '\0';
    fclose(f);
    return uc_load_string(buf, (size_t)sz, arena, out, err);
}