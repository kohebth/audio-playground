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
    advance(L, 2);
    size_t start = L->i;
    while (L->i < L->n && L->src[L->i] != '}') advance(L, 1);
    if (L->i >= L->n) {
        uc_error_set(L->err, UC_E_LEX, loc, "unterminated ${...}");
        return -1;
    }
    size_t len = L->i - start;
    char *txt = uc_arena_strndup(L->arena, L->src + start, len);
    if (!txt) { uc_error_set(L->err, UC_E_OOM, loc, "arena OOM"); return -1; }
    advance(L, 1);

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