#include <apgcore/parser_v2.h>

#include <yaml/lexer.h>
#include <yaml/parser.h>

#include <stdio.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

uc_status apg_v2_parse_string(const char *src, size_t src_len, uc_arena *arena, uc_node **out_root, uc_error *err) {
    if (!src || !arena || !out_root || !err)
        return UC_E_TYPE;
    *out_root   = NULL;
    err->status = UC_OK;

    uc_token_vec tokens = {0};
    uc_status    status = uc_lex(src, src_len, arena, &tokens, err);
    if (status != UC_OK)
        return status;
    return uc_parse(&tokens, arena, out_root, err);
}

uc_status apg_v2_parse_file(const char *path, uc_arena *arena, uc_node **out_root, uc_error *err) {
    if (!path || !arena || !out_root || !err)
        return UC_E_TYPE;

    FILE *file = fopen(path, "rb");
    if (!file) {
        uc_loc loc = {0, 0};
        uc_error_set(err, UC_E_IO, loc, "cannot open '%s'", path);
        return UC_E_IO;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return set_error(err, UC_E_IO, "ftell failed");
    }

    char *buffer = uc_arena_alloc(arena, (size_t)size + 1u, 1u);
    if (!buffer) {
        fclose(file);
        return set_error(err, UC_E_OOM, "arena OOM");
    }
    if (fread(buffer, 1u, (size_t)size, file) != (size_t)size) {
        fclose(file);
        return set_error(err, UC_E_IO, "fread failed");
    }
    buffer[size] = '\0';
    fclose(file);

    return apg_v2_parse_string(buffer, (size_t)size, arena, out_root, err);
}
