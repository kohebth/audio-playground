#include "parser.h"
#include <string.h>

typedef struct {
    const uc_token *t;
    size_t          n;
    size_t          p;
    uc_arena       *arena;
    uc_error       *err;
} ps_t;

static const uc_token *pk(ps_t *P, size_t off) { return &P->t[P->p + off]; }
static const uc_token *eat(ps_t *P)            { return &P->t[P->p++]; }
static int  accept_(ps_t *P, uc_tok_kind k)    { if (pk(P,0)->kind==k){++P->p;return 1;} return 0; }

static int expect(ps_t *P, uc_tok_kind k, const char *what) {
    if (pk(P,0)->kind != k) {
        uc_error_set(P->err, UC_E_PARSE, pk(P,0)->loc, "expected %s", what);
        return -1;
    }
    ++P->p; return 0;
}

static void skip_blank(ps_t *P) {
    for (;;) {
        if (pk(P,0)->kind == UC_TOK_INDENT && pk(P,1)->kind == UC_TOK_NEWLINE) { P->p += 2; continue; }
        if (pk(P,0)->kind == UC_TOK_NEWLINE) { ++P->p; continue; }
        break;
    }
}

static uc_node *node_new(uc_arena *a, uc_node_kind k) {
    uc_node *n = (uc_node *)uc_arena_alloc(a, sizeof *n, sizeof(void*));
    if (n) n->kind = k;
    return n;
}

static int map_push(uc_arena *a, uc_node *m, const char *key, uc_node *v) {
    if (m->map_len == m->map_cap) {
        size_t nc = m->map_cap ? m->map_cap * 2 : 4;
        uc_pair *nb = (uc_pair *)uc_arena_alloc(a, nc * sizeof *nb, sizeof(void*));
        if (!nb) return -1;
        if (m->map) memcpy(nb, m->map, m->map_len * sizeof *nb);
        m->map = nb; m->map_cap = nc;
    }
    m->map[m->map_len].key = key;
    m->map[m->map_len].value = v;
    ++m->map_len;
    return 0;
}

static int seq_push(uc_arena *a, uc_node *s, uc_node *v) {
    if (s->seq_len == s->seq_cap) {
        size_t nc = s->seq_cap ? s->seq_cap * 2 : 4;
        uc_node **nb = (uc_node **)uc_arena_alloc(a, nc * sizeof *nb, sizeof(void*));
        if (!nb) return -1;
        if (s->seq) memcpy(nb, s->seq, s->seq_len * sizeof *nb);
        s->seq = nb; s->seq_cap = nc;
    }
    s->seq[s->seq_len++] = v;
    return 0;
}

static uc_node *parse_block   (ps_t *P, int min_indent);
static uc_node *parse_mapping (ps_t *P, int indent);
static uc_node *parse_sequence(ps_t *P, int indent);
static uc_node *parse_value   (ps_t *P, int parent_indent);
static uc_node *parse_flow_map(ps_t *P);

static uc_node *parse_value(ps_t *P, int parent_indent) {
    const uc_token *t = pk(P, 0);
    switch (t->kind) {
        case UC_TOK_SCALAR: {
            uc_node *n = node_new(P->arena, UC_NODE_SCALAR);
            if (!n) return NULL;
            n->text = t->text; n->text_len = t->len;
            ++P->p; accept_(P, UC_TOK_NEWLINE);
            return n;
        }
        case UC_TOK_VARREF: {
            uc_node *n = node_new(P->arena, UC_NODE_VARREF);
            if (!n) return NULL;
            n->text = t->text; n->text_len = t->len;
            ++P->p; accept_(P, UC_TOK_NEWLINE);
            return n;
        }
        case UC_TOK_NULL:
            ++P->p; accept_(P, UC_TOK_NEWLINE);
            return node_new(P->arena, UC_NODE_NULL);
        case UC_TOK_LBRACE:
            return parse_flow_map(P);
        case UC_TOK_NEWLINE: {
            ++P->p;
            uc_node *child = parse_block(P, parent_indent + 1);
            return child ? child : node_new(P->arena, UC_NODE_NULL);
        }
        default:
            uc_error_set(P->err, UC_E_PARSE, t->loc, "unexpected token in value");
            return NULL;
    }
}

static uc_node *parse_flow_map(ps_t *P) {
    if (expect(P, UC_TOK_LBRACE, "'{'") < 0) return NULL;
    uc_node *m = node_new(P->arena, UC_NODE_MAP);
    if (!m) return NULL;

    if (pk(P,0)->kind != UC_TOK_RBRACE) {
        for (;;) {
            const uc_token *kt = pk(P,0);
            if (kt->kind != UC_TOK_SCALAR) {
                uc_error_set(P->err, UC_E_PARSE, kt->loc, "expected key in flow map");
                return NULL;
            }
            const char *key = kt->text;
            ++P->p;
            if (expect(P, UC_TOK_COLON, "':'") < 0) return NULL;

            uc_node *v = NULL;
            const uc_token *vt = pk(P,0);
            switch (vt->kind) {
                case UC_TOK_VARREF:
                    v = node_new(P->arena, UC_NODE_VARREF);
                    if (!v) return NULL;
                    v->text = vt->text; v->text_len = vt->len; ++P->p; break;
                case UC_TOK_NULL:
                    v = node_new(P->arena, UC_NODE_NULL); ++P->p; break;
                case UC_TOK_LBRACE:
                    v = parse_flow_map(P); break;
                case UC_TOK_SCALAR:
                    v = node_new(P->arena, UC_NODE_SCALAR);
                    if (!v) return NULL;
                    v->text = vt->text; v->text_len = vt->len; ++P->p; break;
                default:
                    uc_error_set(P->err, UC_E_PARSE, vt->loc, "invalid flow value");
                    return NULL;
            }
            if (!v) return NULL;
            if (map_push(P->arena, m, key, v) < 0) {
                uc_error_set(P->err, UC_E_OOM, vt->loc, "arena OOM");
                return NULL;
            }
            if (accept_(P, UC_TOK_COMMA)) continue;
            break;
        }
    }
    if (expect(P, UC_TOK_RBRACE, "'}'") < 0) return NULL;
    accept_(P, UC_TOK_NEWLINE);
    return m;
}

static uc_node *parse_mapping(ps_t *P, int indent) {
    uc_node *m = node_new(P->arena, UC_NODE_MAP);
    if (!m) return NULL;
    for (;;) {
        skip_blank(P);
        if (pk(P,0)->kind != UC_TOK_INDENT || pk(P,0)->indent != indent) break;
        if (pk(P,1)->kind != UC_TOK_SCALAR) break;
        ++P->p;
        const uc_token *kt = eat(P);
        if (expect(P, UC_TOK_COLON, "':' after key") < 0) return NULL;
        uc_node *v = parse_value(P, indent);
        if (!v) return NULL;
        if (map_push(P->arena, m, kt->text, v) < 0) {
            uc_error_set(P->err, UC_E_OOM, kt->loc, "arena OOM");
            return NULL;
        }
    }
    return m;
}

static uc_node *parse_sequence(ps_t *P, int indent) {
    uc_node *s = node_new(P->arena, UC_NODE_SEQ);
    if (!s) return NULL;
    for (;;) {
        skip_blank(P);
        if (pk(P,0)->kind != UC_TOK_INDENT || pk(P,0)->indent != indent) break;
        if (pk(P,1)->kind != UC_TOK_DASH) break;
        P->p += 2;
        int child_indent = indent + 2;

        if (pk(P,0)->kind == UC_TOK_SCALAR && pk(P,1)->kind == UC_TOK_COLON) {
            uc_node *m = node_new(P->arena, UC_NODE_MAP);
            if (!m) return NULL;
            const uc_token *kt = eat(P);
            if (expect(P, UC_TOK_COLON, "':' after list-item key") < 0) return NULL;
            uc_node *v = parse_value(P, child_indent);
            if (!v) return NULL;
            if (map_push(P->arena, m, kt->text, v) < 0) return NULL;

            for (;;) {
                skip_blank(P);
                if (pk(P,0)->kind != UC_TOK_INDENT || pk(P,0)->indent != child_indent) break;
                if (pk(P,1)->kind != UC_TOK_SCALAR) break;
                ++P->p;
                const uc_token *kt2 = eat(P);
                if (expect(P, UC_TOK_COLON, "':' after key") < 0) return NULL;
                uc_node *v2 = parse_value(P, child_indent);
                if (!v2) return NULL;
                if (map_push(P->arena, m, kt2->text, v2) < 0) return NULL;
            }
            if (seq_push(P->arena, s, m) < 0) return NULL;
        } else {
            uc_node *v = parse_value(P, child_indent);
            if (!v) return NULL;
            if (seq_push(P->arena, s, v) < 0) return NULL;
        }
    }
    return s;
}

static uc_node *parse_block(ps_t *P, int min_indent) {
    skip_blank(P);
    if (pk(P,0)->kind != UC_TOK_INDENT) return NULL;
    if (pk(P,0)->indent < min_indent)   return NULL;
    int indent = pk(P,0)->indent;
    return (pk(P,1)->kind == UC_TOK_DASH)
         ? parse_sequence(P, indent)
         : parse_mapping (P, indent);
}

uc_status uc_parse(const uc_token_vec *tokens, uc_arena *arena,
                   uc_node **out_root, uc_error *err)
{
    ps_t P = {0};
    P.t = tokens->data; P.n = tokens->len; P.arena = arena; P.err = err;

    skip_blank(&P);
    uc_node *root = parse_block(&P, 0);
    if (!root) {
        if (err->status == UC_OK) {
            uc_loc loc = {1,1};
            uc_error_set(err, UC_E_PARSE, loc, "empty document");
        }
        return err->status;
    }
    *out_root = root;
    return UC_OK;
}