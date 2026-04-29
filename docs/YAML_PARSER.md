# Professional C port for ARM / embedded targets

This version is **C11**, freestanding-friendly, uses an **arena allocator** (single bump-buffer, freed in one shot), returns **error codes** instead of exceptions, and avoids C++ overhead. It compiles cleanly with `arm-none-eabi-gcc -std=c11 -Wall -Wextra -Wpedantic`.

```
inc/yaml/
    error.h
    arena.h
    token.h
    node.h
    lexer.h
    parser.h
    unit.h
    loader.h
src/yaml/
    arena.c
    lexer.c
    parser.c
    loader.c
    main.c
CMakeLists.txt
```

---

## `inc/yaml/error.h`

```c++
#ifndef UNITCFG_ERROR_H
#define UNITCFG_ERROR_H

typedef enum {
    UC_OK = 0,
    UC_E_IO,
    UC_E_OOM,
    UC_E_LEX,
    UC_E_PARSE,
    UC_E_TYPE,
    UC_E_RANGE,
    UC_E_MISSING
} uc_status;

typedef struct {
    int line;
    int col;
} uc_loc;

typedef struct {
    uc_status status;
    uc_loc    loc;
    char      msg[128];
} uc_error;

void uc_error_set(uc_error *e, uc_status s, uc_loc loc, const char *fmt, ...);
const char *uc_status_str(uc_status s);

#endif
```

---

## `inc/yaml/arena.h`

```c++
#ifndef UNITCFG_ARENA_H
#define UNITCFG_ARENA_H

#include <stddef.h>

/*
 * Bump arena: one contiguous buffer, O(1) allocation, freed all at once.
 * Suitable for parsing where lifetime == "until we’re done".
 */
typedef struct uc_arena {
    unsigned char *base;
    size_t         size;
    size_t         used;
    int            owns_buffer;   /* 1 if we malloc'd, 0 if user-supplied */
} uc_arena;

/* Heap-backed (uses malloc once). */
int   uc_arena_init      (uc_arena *a, size_t bytes);
/* User-provided storage (no malloc, ideal for ARM/MCU). */
void  uc_arena_init_fixed(uc_arena *a, void *buf, size_t bytes);
void  uc_arena_free      (uc_arena *a);
void  uc_arena_reset     (uc_arena *a);
void *uc_arena_alloc     (uc_arena *a, size_t bytes, size_t align);
char *uc_arena_strndup   (uc_arena *a, const char *s, size_t n);

#endif
```

---

## `src/yaml/arena.c`

```c++
#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int uc_arena_init(uc_arena *a, size_t bytes) {
    a->base = (unsigned char *)malloc(bytes);
    if (!a->base) return -1;
    a->size = bytes;
    a->used = 0;
    a->owns_buffer = 1;
    return 0;
}

void uc_arena_init_fixed(uc_arena *a, void *buf, size_t bytes) {
    a->base = (unsigned char *)buf;
    a->size = bytes;
    a->used = 0;
    a->owns_buffer = 0;
}

void uc_arena_free(uc_arena *a) {
    if (a->owns_buffer && a->base) free(a->base);
    a->base = NULL; a->size = a->used = 0; a->owns_buffer = 0;
}

void uc_arena_reset(uc_arena *a) { a->used = 0; }

void *uc_arena_alloc(uc_arena *a, size_t bytes, size_t align) {
    if (align == 0) align = sizeof(void *);
    uintptr_t p   = (uintptr_t)(a->base + a->used);
    uintptr_t pad = (align - (p & (align - 1))) & (align - 1);
    if (a->used + pad + bytes > a->size) return NULL;   /* OOM */
    void *out = a->base + a->used + pad;
    a->used += pad + bytes;
    memset(out, 0, bytes);
    return out;
}

char *uc_arena_strndup(uc_arena *a, const char *s, size_t n) {
    char *p = (char *)uc_arena_alloc(a, n + 1, 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
```

---

## `inc/yaml/token.h`

```c++
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
    const char *text;   /* arena-owned, NUL-terminated; NULL when unused */
    int         len;    /* text length (excl. NUL) */
    int         indent; /* for UC_TOK_INDENT */
    uc_loc      loc;
} uc_token;

#endif
```

---

## `inc/yaml/node.h`

```c++
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

/* Ordered map entry */
typedef struct {
    const char *key;
    uc_node    *value;
} uc_pair;

struct uc_node {
    uc_node_kind kind;

    /* SCALAR / VARREF */
    const char *text;
    int         text_len;

    /* MAP */
    uc_pair *map;
    size_t   map_len;
    size_t   map_cap;

    /* SEQ */
    uc_node **seq;
    size_t    seq_len;
    size_t    seq_cap;
};

const uc_node *uc_node_find(const uc_node *n, const char *key);

#endif
```

---

## `inc/yaml/lexer.h`

```c++
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
```

---

## `src/yaml/lexer.c`

```c++
#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char  *src;
    size_t       n;
    size_t       i;
    int          line;
    int          col;
    int          flow;
    int          line_start;
    uc_arena    *arena;
    uc_token_vec*out;
    uc_error    *err;
} lex_t;

static int vec_push(uc_arena *a, uc_token_vec *v, uc_token t) {
    if (v->len == v->cap) {
        size_t nc = v->cap ? v->cap * 2 : 64;
        uc_token *nb = (uc_token *)uc_arena_alloc(a, nc * sizeof *nb, sizeof(void*));
        if (!nb) return -1;
        if (v->data) memcpy(nb, v->data, v->len * sizeof *nb);
        v->data = nb; v->cap = nc;
    }
    v->data[v->len++] = t;
    return 0;
}

static void advance(lex_t *L, size_t k) {
    for (size_t z = 0; z < k && L->i < L->n; ++z, ++L->i) {
        if (L->src[L->i] == '\n') { ++L->line; L->col = 1; }
        else                       { ++L->col; }
    }
}

static char peek(lex_t *L, size_t off) {
    return (L->i + off < L->n) ? L->src[L->i + off] : '\0';
}

static int emit_simple(lex_t *L, uc_tok_kind k) {
    uc_token t = {0};
    t.kind = k;
    t.loc.line = L->line; t.loc.col = L->col;
    return vec_push(L->arena, L->out, t);
}

static int emit_indent(lex_t *L) {
    int n = 0;
    uc_loc loc = {L->line, L->col};
    while (peek(L, 0) == ' ') { ++n; advance(L, 1); }
    uc_token t = {0};
    t.kind = UC_TOK_INDENT; t.indent = n; t.loc = loc;
    L->line_start = 0;
    return vec_push(L->arena, L->out, t);
}

static void skip_comment(lex_t *L) {
    while (L->i < L->n && L->src[L->i] != '\n') advance(L, 1);
}

static int lex_varref(lex_t *L) {
    uc_loc loc = {L->line, L->col};
    advance(L, 2);                              /* consume "${" */
    size_t start = L->i;
    while (L->i < L->n && L->src[L->i] != '}') advance(L, 1);
    if (L->i >= L->n) {
        uc_error_set(L->err, UC_E_LEX, loc, "unterminated ${...}");
        return -1;
    }
    size_t len = L->i - start;
    char *txt = uc_arena_strndup(L->arena, L->src + start, len);
    if (!txt) { uc_error_set(L->err, UC_E_OOM, loc, "arena OOM"); return -1; }
    advance(L, 1);                              /* consume '}' */

    uc_token t = {0};
    t.kind = UC_TOK_VARREF; t.text = txt; t.len = (int)len; t.loc = loc;
    return vec_push(L->arena, L->out, t);
}

static int lex_scalar(lex_t *L) {
    uc_loc loc = {L->line, L->col};
    size_t start = L->i;
    while (L->i < L->n) {
        char c = L->src[L->i];
        if (c == '\n' || c == '#') break;
        if (L->flow && (c == ',' || c == '}')) break;
        if (!L->flow && c == ':' &&
            (L->i + 1 == L->n || isspace((unsigned char)L->src[L->i + 1])))
            break;
        advance(L, 1);
    }
    size_t end = L->i;
    while (end > start && (L->src[end-1] == ' ' || L->src[end-1] == '\t'))
        --end;
    size_t len = end - start;
    if (len == 0) return 0;

    char *txt = uc_arena_strndup(L->arena, L->src + start, len);
    if (!txt) { uc_error_set(L->err, UC_E_OOM, loc, "arena OOM"); return -1; }

    uc_token t = {0};
    t.kind = UC_TOK_SCALAR; t.text = txt; t.len = (int)len; t.loc = loc;
    return vec_push(L->arena, L->out, t);
}

uc_status uc_lex(const char *src, size_t src_len,
                 uc_arena *arena, uc_token_vec *out, uc_error *err)
{
    lex_t L = {0};
    L.src = src; L.n = src_len;
    L.line = 1; L.col = 1; L.line_start = 1;
    L.arena = arena; L.out = out; L.err = err;

    while (L.i < L.n) {
        if (L.line_start && L.flow == 0) { if (emit_indent(&L) < 0) return err->status; continue; }

        char c = L.src[L.i];
        switch (c) {
            case '\n':
                if (L.flow == 0 && emit_simple(&L, UC_TOK_NEWLINE) < 0) return UC_E_OOM;
                advance(&L, 1); L.line_start = 1; continue;

            case ' ': case '\t': advance(&L, 1); continue;

            case '#': skip_comment(&L); continue;

            case '{':
                if (emit_simple(&L, UC_TOK_LBRACE) < 0) return UC_E_OOM;
                ++L.flow; advance(&L, 1); continue;

            case '}':
                if (emit_simple(&L, UC_TOK_RBRACE) < 0) return UC_E_OOM;
                --L.flow; advance(&L, 1); continue;

            case ',':
                if (emit_simple(&L, UC_TOK_COMMA) < 0) return UC_E_OOM;
                advance(&L, 1); continue;

            case ':': {
                char nx = peek(&L, 1);
                if (L.flow || nx == '\0' || isspace((unsigned char)nx)) {
                    if (emit_simple(&L, UC_TOK_COLON) < 0) return UC_E_OOM;
                    advance(&L, 1); continue;
                }
                break;
            }
            case '-':
                if (peek(&L, 1) == ' ' && L.flow == 0) {
                    if (emit_simple(&L, UC_TOK_DASH) < 0) return UC_E_OOM;
                    advance(&L, 1); continue;
                }
                break;

            case '~': {
                char nx = peek(&L, 1);
                if (nx == '\0' || nx == '\n' || nx == ',' || nx == '}' ||
                    isspace((unsigned char)nx)) {
                    if (emit_simple(&L, UC_TOK_NULL) < 0) return UC_E_OOM;
                    advance(&L, 1); continue;
                }
                break;
            }
            case '$':
                if (peek(&L, 1) == '{') { if (lex_varref(&L) < 0) return err->status; continue; }
                break;
        }
        if (lex_scalar(&L) < 0) return err->status;
    }

    uc_token eof = {0};
    eof.kind = UC_TOK_EOF; eof.loc.line = L.line; eof.loc.col = L.col;
    if (vec_push(arena, out, eof) < 0) return UC_E_OOM;
    return UC_OK;
}
```

---

## `inc/yaml/parser.h`

```c++
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
```

---

## `src/yaml/parser.c`

```c++
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

/* forward decls */
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
        ++P->p;                                    /* INDENT */
        const uc_token *kt = eat(P);               /* SCALAR */
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
```

---

## `inc/yaml/unit.h`

```c++
#ifndef UNITCFG_UNIT_H
#define UNITCFG_UNIT_H

#include <stddef.h>

typedef enum { UC_VAL_LITERAL, UC_VAL_VARREF } uc_value_kind;

typedef struct {
    uc_value_kind kind;
    const char   *text;     /* literal text or "a.b.c" path */
} uc_value;

typedef struct {
    const char *key;
    uc_value    value;
} uc_kv;

typedef struct {
    const char *key;
    double      value;
} uc_kv_d;

typedef struct {
    const char *name;
    double      min;
    double      max;
    double      def;
    const char *unit;
} uc_param;

typedef struct {
    const char *id;
    const char *fn;

    uc_kv      *in;     size_t in_len;
    uc_kv      *out;    size_t out_len;
    uc_kv      *config; size_t config_len;
    uc_kv_d    *state;  size_t state_len;
} uc_stage;

typedef struct {
    int sample_rate;
    int channel;
} uc_system;

typedef struct {
    const char  *name;
    const char  *version;
    uc_system    system;

    uc_param    *params;   size_t params_len;
    uc_kv_d     *internal; size_t internal_len;

    const char **signals;  size_t signals_len;

    uc_stage    *pipeline; size_t pipeline_len;
} uc_unit;

#endif
```

---

## `inc/yaml/loader.h`

```c++
#ifndef UNITCFG_LOADER_H
#define UNITCFG_LOADER_H

#include "arena.h"
#include "error.h"
#include "unit.h"

/*
 * Loads a Unit from text. All strings/arrays inside `out` are owned by
 * `arena`; calling uc_arena_free(arena) releases everything at once.
 */
uc_status uc_load_string(const char *src, size_t src_len,
                         uc_arena *arena, uc_unit *out, uc_error *err);

uc_status uc_load_file  (const char *path,
                         uc_arena *arena, uc_unit *out, uc_error *err);

#endif
```

---

## `src/yaml/loader.c`

```c++
#include "loader.h"
#include "lexer.h"
#include "parser.h"
#include "node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers ---------- */

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

/* ---------- map -> kv arrays ---------- */

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

/* ---------- builders ---------- */

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

/* ---------- public API ---------- */

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
```

---

## `src/yaml/error_and_node.c` *(small shared utilities — put alongside others)*

```c++
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
```

---

## Usage example

```c++
#include "loader.h"
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.unit.yaml>\n", argv[0]);
        return 2;
    }

    /* On a host: 64 KiB heap arena. On an MCU: pass a static buffer instead. */
    uc_arena arena;
    if (uc_arena_init(&arena, 64 * 1024) != 0) {
        fprintf(stderr, "arena init failed\n");
        return 1;
    }

    uc_unit  unit;
    uc_error err = {0};
    if (uc_load_file(argv[1], &arena, &unit, &err) != UC_OK) {
        fprintf(stderr, "%s: [%d:%d] %s\n",
                uc_status_str(err.status), err.loc.line, err.loc.col, err.msg);
        uc_arena_free(&arena);
        return 1;
    }

    printf("Unit: %s v%s  sr=%d ch=%d\n",
           unit.name ? unit.name : "?",
           unit.version ? unit.version : "?",
           unit.system.sample_rate, unit.system.channel);
    printf("Params=%zu Signals=%zu Stages=%zu\n",
           unit.params_len, unit.signals_len, unit.pipeline_len);

    for (size_t i = 0; i < unit.pipeline_len; ++i) {
        const uc_stage *s = &unit.pipeline[i];
        printf("  - %s (%s) in=%zu out=%zu cfg=%zu state=%zu\n",
               s->id ? s->id : "?", s->fn ? s->fn : "?",
               s->in_len, s->out_len, s->config_len, s->state_len);
    }

    uc_arena_free(&arena);
    return 0;
}
```
