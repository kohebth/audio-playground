#include <apgcore/host_v2.h>

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

uc_status apg_v2_host_load_file(
    const char *path, uint32_t frame_capacity, float sample_rate, apg_v2_host_unit_t *out, uc_error *err
) {
    if (!path || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    if (uc_arena_init(&out->arena, 1024u * 1024u) != 0)
        return set_error(err, UC_E_OOM, "v2 host arena allocation failed");
    out->arena_ready = true;

    uc_status status = apg_unit_v2_load_file(path, &out->arena, &out->unit, err);
    if (status != UC_OK)
        goto fail;

    status = apg_v2_compile_unit(&out->unit, &out->arena, &out->plan, err);
    if (status != UC_OK)
        goto fail;

    status = apg_v2_runtime_image_build_with_growth(
        &out->plan, frame_capacity, sample_rate, &out->image_arena, &out->image, err
    );
    if (status != UC_OK)
        goto fail;

    status = apg_v2_runtime_init_from_image(&out->image, &out->runtime, err);
    if (status != UC_OK)
        goto fail;

    out->image_arena_ready = true;
    out->runtime_ready     = true;
    return UC_OK;

fail:
    apg_v2_host_destroy(out);
    return status;
}

bool apg_v2_host_set_param(apg_v2_host_unit_t *host, const char *name, float value) {
    if (!host || !host->runtime_ready)
        return false;
    return apg_v2_runtime_set_param(&host->runtime, name, value);
}

bool apg_v2_host_process_mono_ports(
    apg_v2_host_unit_t *host,
    const char         *input_port_name,
    const float        *input,
    const char         *output_port_name,
    float              *output,
    uint32_t            frames
) {
    if (!host || !host->runtime_ready)
        return false;
    return apg_v2_runtime_process_mono_ports(&host->runtime, input_port_name, input, output_port_name, output, frames);
}

void apg_v2_host_destroy(apg_v2_host_unit_t *host) {
    if (!host)
        return;
    if (host->runtime_ready)
        apg_v2_runtime_destroy(&host->runtime);
    if (host->image_arena_ready)
        uc_arena_free(&host->image_arena);
    if (host->arena_ready)
        uc_arena_free(&host->arena);
    memset(host, 0, sizeof(*host));
}

uc_status apg_v2_host_project_load_file(
    const char *path, uint32_t frame_capacity, float sample_rate, apg_v2_host_project_t *out, uc_error *err
) {
    if (!path || !out || !err)
        return UC_E_TYPE;
    memset(out, 0, sizeof(*out));
    err->status = UC_OK;

    if (uc_arena_init(&out->arena, 2 * 1024 * 1024u) != 0)
        return set_error(err, UC_E_OOM, "v2 host arena allocation failed");
    out->arena_ready = true;

    uc_status status = apg_project_v2_load_resolved_file(path, &out->arena, &out->resolved_project, err);
    if (status != UC_OK)
        goto fail;

    status = apg_project_v2_compile(&out->resolved_project, &out->arena, &out->compiled, err);
    if (status != UC_OK)
        goto fail;

    status = apg_v2_runtime_image_build_with_growth(
        &out->compiled.plan, frame_capacity, sample_rate, &out->image_arena, &out->image, err
    );
    if (status != UC_OK)
        goto fail;

    status = apg_v2_runtime_init_from_image(&out->image, &out->runtime, err);
    if (status != UC_OK)
        goto fail;

    out->image_arena_ready = true;
    out->runtime_ready     = true;
    return UC_OK;

fail:
    apg_v2_host_project_destroy(out);
    return status;
}

bool apg_v2_host_project_set_param(apg_v2_host_project_t *host, const char *name, float value) {
    if (!host || !host->runtime_ready)
        return false;
    return apg_v2_runtime_set_param(&host->runtime, name, value);
}

bool apg_v2_host_project_process_mono_ports(
    apg_v2_host_project_t *host,
    const char            *input_port_name,
    const float           *input,
    const char            *output_port_name,
    float                 *output,
    uint32_t               frames
) {
    if (!host || !host->runtime_ready)
        return false;
    return apg_v2_runtime_process_mono_ports(&host->runtime, input_port_name, input, output_port_name, output, frames);
}

void apg_v2_host_project_destroy(apg_v2_host_project_t *host) {
    if (!host)
        return;
    if (host->runtime_ready)
        apg_v2_runtime_destroy(&host->runtime);
    if (host->image_arena_ready)
        uc_arena_free(&host->image_arena);
    if (host->arena_ready)
        uc_arena_free(&host->arena);
    memset(host, 0, sizeof(*host));
}
