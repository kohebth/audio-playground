#include <apgcore/compiler/compiler_v2.h>
#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/host/host_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/validator/project_v2.h>
#include <apgcore/validator/unit_v2.h>

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct apg_v2_host_unit {
    uc_arena               arena;
    apg_unit_v2_t          unit;
    apg_v2_compiled_unit_t plan;
    apg_v2_registry_t      registry;
    uc_arena               registry_arena;
    apg_v2_runtime_t      *runtime;
    bool                   arena_ready;
    bool                   registry_ready;
    bool                   runtime_ready;
};

struct apg_v2_host_project {
    uc_arena                  arena;
    apg_project_v2_resolved_t resolved_project;
    apg_project_v2_compiled_t compiled;
    apg_v2_registry_t         registry;
    uc_arena                  registry_arena;
    apg_v2_runtime_t         *runtime;
    bool                      arena_ready;
    bool                      registry_ready;
    bool                      runtime_ready;
};

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static uc_status build_runtime_from_plan(
    const apg_v2_compiled_unit_t *plan,
    uint32_t                      frame_capacity,
    float                         sample_rate,
    uc_arena                     *registry_arena,
    apg_v2_registry_t            *registry,
    apg_v2_runtime_t            **runtime,
    uc_error                     *err
) {
    if (!plan || !registry_arena || !registry || !runtime || !err)
        return UC_E_TYPE;
    *runtime = NULL;

    uc_status status =
        apg_v2_registry_build_with_growth(plan, frame_capacity, sample_rate, registry_arena, registry, err);
    if (status != UC_OK)
        return status;

    status = apg_v2_runtime_create_from_registry(registry, runtime, err);
    if (status != UC_OK) {
        return status;
    }
    return UC_OK;
}

uc_status apg_v2_host_load_file(
    const char *path, uint32_t frame_capacity, float sample_rate, apg_v2_host_unit_t **out, uc_error *err
) {
    if (!path || !out || !err)
        return UC_E_TYPE;
    *out        = NULL;
    err->status = UC_OK;

    apg_v2_host_unit_t *host = calloc(1u, sizeof(*host));
    if (!host)
        return set_error(err, UC_E_OOM, "v2 host allocation failed");

    if (uc_arena_init(&host->arena, 1024u * 1024u) != 0) {
        free(host);
        return set_error(err, UC_E_OOM, "v2 host arena allocation failed");
    }
    host->arena_ready = true;

    uc_status status = apg_unit_v2_load_file(path, &host->arena, &host->unit, err);
    if (uc_arena_init(&host->registry_arena, 1024u * 1024u) != 0) {
        status = set_error(err, UC_E_OOM, "v2 host registry arena allocation failed");
        goto fail;
    }
    host->registry_ready = true;
    if (status != UC_OK)
        goto fail;

    status = apg_v2_compile_unit(&host->unit, &host->arena, &host->plan, err);
    if (status != UC_OK)
        goto fail;

    status = build_runtime_from_plan(
        &host->plan, frame_capacity, sample_rate, &host->registry_arena, &host->registry, &host->runtime, err
    );
    if (status != UC_OK)
        goto fail;

    host->runtime_ready = true;
    *out                = host;
    return UC_OK;

fail:
    apg_v2_host_destroy(host);
    return status;
}

bool apg_v2_host_set_param(apg_v2_host_unit_t *host, const char *name, float value) {
    if (!host || !host->runtime_ready)
        return false;
    return apg_v2_runtime_set_param(host->runtime, name, value);
}

const char *apg_v2_host_last_error(const apg_v2_host_unit_t *host) {
    return host ? apg_v2_measure_last_error(host->runtime) : NULL;
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
    return apg_v2_runtime_process_mono_ports(host->runtime, input_port_name, input, output_port_name, output, frames);
}

void apg_v2_host_destroy(apg_v2_host_unit_t *host) {
    if (!host)
        return;
    if (host->runtime) {
        apg_v2_runtime_destroy_owned(&host->runtime);
    }
    if (host->registry_ready)
        uc_arena_free(&host->registry_arena);
    if (host->arena_ready)
        uc_arena_free(&host->arena);
    free(host);
}

uc_status apg_v2_host_project_load_file(
    const char *path, uint32_t frame_capacity, float sample_rate, apg_v2_host_project_t **out, uc_error *err
) {
    if (!path || !out || !err)
        return UC_E_TYPE;
    *out        = NULL;
    err->status = UC_OK;

    apg_v2_host_project_t *host = calloc(1u, sizeof(*host));
    if (!host)
        return set_error(err, UC_E_OOM, "v2 host allocation failed");

    if (uc_arena_init(&host->arena, 2 * 1024 * 1024u) != 0) {
        free(host);
        return set_error(err, UC_E_OOM, "v2 host arena allocation failed");
    }
    host->arena_ready = true;

    uc_status status = apg_project_v2_load_resolved_file(path, &host->arena, &host->resolved_project, err);
    if (uc_arena_init(&host->registry_arena, 2 * 1024 * 1024u) != 0) {
        status = set_error(err, UC_E_OOM, "v2 host registry arena allocation failed");
        goto fail;
    }
    host->registry_ready = true;
    if (status != UC_OK)
        goto fail;

    status = apg_project_v2_compile(&host->resolved_project, &host->arena, &host->compiled, err);
    if (status != UC_OK)
        goto fail;

    status = build_runtime_from_plan(
        &host->compiled.plan, frame_capacity, sample_rate, &host->registry_arena, &host->registry, &host->runtime, err
    );
    if (status != UC_OK)
        goto fail;

    host->runtime_ready = true;
    *out                = host;
    return UC_OK;

fail:
    apg_v2_host_project_destroy(host);
    return status;
}

bool apg_v2_host_project_set_param(apg_v2_host_project_t *host, const char *name, float value) {
    if (!host || !host->runtime_ready)
        return false;
    return apg_v2_runtime_set_param(host->runtime, name, value);
}

const char *apg_v2_host_project_last_error(const apg_v2_host_project_t *host) {
    return host ? apg_v2_measure_last_error(host->runtime) : NULL;
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
    return apg_v2_runtime_process_mono_ports(host->runtime, input_port_name, input, output_port_name, output, frames);
}

void apg_v2_host_project_destroy(apg_v2_host_project_t *host) {
    if (!host)
        return;
    if (host->runtime) {
        apg_v2_runtime_destroy_owned(&host->runtime);
    }
    if (host->registry_ready)
        uc_arena_free(&host->registry_arena);
    if (host->arena_ready)
        uc_arena_free(&host->arena);
    free(host);
}
