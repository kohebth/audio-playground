#include <apgcore/unit_v2.h>

#include <apgcore/parser_v2.h>
#include <apgcore/unit_validator_v2.h>

#include <string.h>

uc_status apg_unit_v2_load_string(const char *src, size_t src_len, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!src || !arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_node  *root   = NULL;
    uc_status status = apg_v2_parse_string(src, src_len, arena, &root, err);
    if (status != UC_OK)
        return status;
    return apg_unit_v2_validate_root(root, arena, out, err);
}

uc_status apg_unit_v2_load_file(const char *path, uc_arena *arena, apg_unit_v2_t *out, uc_error *err) {
    if (!arena || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    uc_node  *root   = NULL;
    uc_status status = apg_v2_parse_file(path, arena, &root, err);
    if (status != UC_OK)
        return status;
    return apg_unit_v2_validate_root(root, arena, out, err);
}
