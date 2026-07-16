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

#define APG_V2_HOST_SWAP_CROSSFADE_FRAMES 64u

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

typedef struct {
    char *name;
    float value;
} apg_v2_host_param_shadow_t;

typedef struct {
    char *instance_id;
    bool  enabled;
} apg_v2_host_bypass_shadow_t;

typedef struct apg_v2_host_project_bundle {
    uc_arena                  arena;
    apg_project_v2_compiled_t compiled;
    apg_v2_registry_t         registry;
    uc_arena                  registry_arena;
    apg_v2_runtime_t         *runtime;
    bool                      arena_ready;
    bool                      registry_ready;
    bool                      runtime_ready;
} apg_v2_host_project_bundle_t;

struct apg_v2_host_project_swap {
    apg_v2_host_project_bundle_t *bundle;
};

struct apg_v2_host_project {
    apg_v2_host_project_bundle_t *active;
    apg_v2_host_project_bundle_t *fade_out;
    float                        *crossfade_old;
    float                        *crossfade_new;
    apg_prepare_context_t         prepare_context;
    uint32_t                      crossfade_total_frames;
    uint32_t                      crossfade_remaining_frames;
    uint32_t                      crossfade_offset_frames;
    apg_v2_host_param_shadow_t   *param_shadows;
    size_t                        param_shadows_len;
    apg_v2_host_bypass_shadow_t  *bypass_shadows;
    size_t                        bypass_shadows_len;
    bool                          project_muted;
};

static uc_status set_error(uc_error *err, uc_status status, const char *msg) {
    uc_loc loc = {0, 0};
    uc_error_set(err, status, loc, "%s", msg);
    return status;
}

static uc_status build_runtime_from_plan(
    const apg_v2_compiled_unit_t *plan,
    const apg_prepare_context_t  *prepare_context,
    uc_arena                     *registry_arena,
    apg_v2_registry_t            *registry,
    apg_v2_runtime_t            **runtime,
    uc_error                     *err
) {
    if (!plan || !registry_arena || !registry || !runtime || !err)
        return UC_E_TYPE;
    *runtime = NULL;

    uc_status status = apg_v2_registry_build_with_growth(plan, prepare_context, registry_arena, registry, err);
    if (status != UC_OK)
        return status;

    status = apg_v2_runtime_create_from_registry(registry, runtime, err);
    if (status != UC_OK) {
        return status;
    }
    return UC_OK;
}

static bool registry_param_index_by_name(const apg_v2_registry_t *registry, const char *name, size_t *out_index) {
    if (!registry || !name || !out_index)
        return false;
    for (size_t i = 0; i < registry->params_len; i++) {
        if (registry->param_names[i] && strcmp(registry->param_names[i], name) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool
registry_bypass_index_by_instance_id(const apg_v2_registry_t *registry, const char *instance_id, size_t *out_index) {
    if (!registry || !instance_id || !out_index)
        return false;
    size_t instance_id_len = strlen(instance_id);
    for (size_t i = 0; i < registry->bypassed_instances_len; i++) {
        if (registry->bypass_instances[i].instance_id &&
            registry->bypass_instances[i].instance_id_len == instance_id_len &&
            strncmp(registry->bypass_instances[i].instance_id, instance_id, instance_id_len) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static bool registry_audio_port_index_by_name(
    const apg_v2_registry_audio_port_t *ports, size_t ports_len, const char *port_name, size_t *out_index
) {
    if (!ports || !port_name || !out_index)
        return false;
    for (size_t i = 0; i < ports_len; i++) {
        if (ports[i].port_name && strcmp(ports[i].port_name, port_name) == 0) {
            *out_index = i;
            return true;
        }
    }
    return false;
}

static char *host_strdup(const char *text) {
    if (!text)
        return NULL;
    size_t len  = strlen(text);
    char  *copy = malloc(len + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
}

static bool
host_runtime_set_param(const apg_v2_registry_t *registry, apg_v2_runtime_t *runtime, const char *name, float value) {
    size_t param_index = 0u;
    return registry_param_index_by_name(registry, name, &param_index) &&
           apg_v2_runtime_set_param_index(runtime, param_index, value);
}

static bool host_runtime_set_bypass(
    const apg_v2_registry_t *registry, apg_v2_runtime_t *runtime, const char *instance_id, bool enabled
) {
    size_t bypass_index = 0u;
    return registry_bypass_index_by_instance_id(registry, instance_id, &bypass_index) &&
           apg_v2_runtime_set_instance_bypass_index(runtime, bypass_index, enabled);
}

static bool host_runtime_process_mono_ports(
    const apg_v2_registry_t *registry,
    apg_v2_runtime_t        *runtime,
    const char              *input_port_name,
    apg_const_buffer_t       input,
    const char              *output_port_name,
    apg_buffer_t             output,
    uint32_t                 frames
) {
    size_t input_index  = 0u;
    size_t output_index = 0u;
    return registry_audio_port_index_by_name(
               registry->input_audio_ports, registry->input_audio_ports_len, input_port_name, &input_index
           ) &&
           registry_audio_port_index_by_name(
               registry->output_audio_ports, registry->output_audio_ports_len, output_port_name, &output_index
           ) &&
           apg_v2_runtime_process_mono_port_indices(runtime, input_index, input, output_index, output, frames);
}

static void host_project_bundle_destroy(apg_v2_host_project_bundle_t **bundle) {
    if (!bundle || !*bundle)
        return;
    apg_v2_host_project_bundle_t *item = *bundle;
    if (item->runtime)
        apg_v2_runtime_destroy_owned(&item->runtime);
    if (item->registry_ready)
        uc_arena_free(&item->registry_arena);
    if (item->arena_ready)
        uc_arena_free(&item->arena);
    free(item);
    *bundle = NULL;
}

static uc_status host_project_bundle_create(
    const apg_project_v2_resolved_t *project,
    const apg_prepare_context_t     *prepare_context,
    apg_v2_host_project_bundle_t   **out,
    uc_error                        *err
) {
    if (!project || !out || !err)
        return UC_E_TYPE;
    *out = NULL;

    apg_v2_host_project_bundle_t *bundle = calloc(1u, sizeof(*bundle));
    if (!bundle)
        return set_error(err, UC_E_OOM, "v2 host project bundle allocation failed");

    if (uc_arena_init(&bundle->arena, 2 * 1024 * 1024u) != 0) {
        host_project_bundle_destroy(&bundle);
        return set_error(err, UC_E_OOM, "v2 host project bundle arena allocation failed");
    }
    bundle->arena_ready = true;

    uc_status status = apg_project_v2_compile(project, &bundle->arena, &bundle->compiled, err);
    if (status != UC_OK) {
        host_project_bundle_destroy(&bundle);
        return status;
    }

    if (uc_arena_init(&bundle->registry_arena, 2 * 1024 * 1024u) != 0) {
        host_project_bundle_destroy(&bundle);
        return set_error(err, UC_E_OOM, "v2 host project registry arena allocation failed");
    }
    bundle->registry_ready = true;

    status = build_runtime_from_plan(
        &bundle->compiled.plan, prepare_context, &bundle->registry_arena, &bundle->registry, &bundle->runtime, err
    );
    if (status != UC_OK) {
        host_project_bundle_destroy(&bundle);
        return status;
    }

    bundle->runtime_ready = true;
    *out                  = bundle;
    return UC_OK;
}

static bool host_project_shadow_param(apg_v2_host_project_t *host, const char *name, float value) {
    if (!host || !name)
        return false;
    for (size_t i = 0; i < host->param_shadows_len; i++) {
        if (host->param_shadows[i].name && strcmp(host->param_shadows[i].name, name) == 0) {
            host->param_shadows[i].value = value;
            return true;
        }
    }

    apg_v2_host_param_shadow_t *items =
        realloc(host->param_shadows, (host->param_shadows_len + 1u) * sizeof(*host->param_shadows));
    if (!items)
        return false;
    host->param_shadows = items;

    char *copy = host_strdup(name);
    if (!copy)
        return false;
    host->param_shadows[host->param_shadows_len++] = (apg_v2_host_param_shadow_t){
        .name  = copy,
        .value = value,
    };
    return true;
}

static bool host_project_shadow_bypass(apg_v2_host_project_t *host, const char *instance_id, bool enabled) {
    if (!host || !instance_id)
        return false;
    for (size_t i = 0; i < host->bypass_shadows_len; i++) {
        if (host->bypass_shadows[i].instance_id && strcmp(host->bypass_shadows[i].instance_id, instance_id) == 0) {
            host->bypass_shadows[i].enabled = enabled;
            return true;
        }
    }

    apg_v2_host_bypass_shadow_t *items =
        realloc(host->bypass_shadows, (host->bypass_shadows_len + 1u) * sizeof(*host->bypass_shadows));
    if (!items)
        return false;
    host->bypass_shadows = items;

    char *copy = host_strdup(instance_id);
    if (!copy)
        return false;
    host->bypass_shadows[host->bypass_shadows_len++] = (apg_v2_host_bypass_shadow_t){
        .instance_id = copy,
        .enabled     = enabled,
    };
    return true;
}

static void host_project_free_shadows(apg_v2_host_project_t *host) {
    if (!host)
        return;
    for (size_t i = 0; i < host->param_shadows_len; i++)
        free(host->param_shadows[i].name);
    free(host->param_shadows);
    host->param_shadows     = NULL;
    host->param_shadows_len = 0u;

    for (size_t i = 0; i < host->bypass_shadows_len; i++)
        free(host->bypass_shadows[i].instance_id);
    free(host->bypass_shadows);
    host->bypass_shadows     = NULL;
    host->bypass_shadows_len = 0u;
}

static void host_project_apply_shadows(apg_v2_host_project_t *host, apg_v2_host_project_bundle_t *bundle) {
    if (!host || !bundle || !bundle->runtime_ready)
        return;
    for (size_t i = 0; i < host->param_shadows_len; i++) {
        (void)host_runtime_set_param(
            &bundle->registry, bundle->runtime, host->param_shadows[i].name, host->param_shadows[i].value
        );
    }
    for (size_t i = 0; i < host->bypass_shadows_len; i++) {
        (void)host_runtime_set_bypass(
            &bundle->registry, bundle->runtime, host->bypass_shadows[i].instance_id, host->bypass_shadows[i].enabled
        );
    }
    (void)apg_v2_runtime_set_project_mute(bundle->runtime, host->project_muted);
}

static bool host_project_alloc_crossfade_buffers(apg_v2_host_project_t *host) {
    if (!host || host->prepare_context.maximum_frames == 0u)
        return true;
    host->crossfade_old = calloc(host->prepare_context.maximum_frames, sizeof(*host->crossfade_old));
    host->crossfade_new = calloc(host->prepare_context.maximum_frames, sizeof(*host->crossfade_new));
    return host->crossfade_old && host->crossfade_new;
}

uc_status apg_v2_host_load_file(
    const char *path, const apg_prepare_context_t *prepare_context, apg_v2_host_unit_t **out, uc_error *err
) {
    if (!path || !out || !err)
        return UC_E_TYPE;
    *out        = NULL;
    err->status = UC_OK;
    if (!apg_prepare_context_valid(prepare_context))
        return set_error(err, UC_E_RANGE, "v2 host prepare context is invalid");

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
        &host->plan, prepare_context, &host->registry_arena, &host->registry, &host->runtime, err
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
    return host_runtime_set_param(&host->registry, host->runtime, name, value);
}

const char *apg_v2_host_last_error(const apg_v2_host_unit_t *host) {
    return host ? apg_v2_measure_last_error(host->runtime) : NULL;
}

bool apg_v2_host_process_mono_ports(
    apg_v2_host_unit_t *host,
    const char         *input_port_name,
    apg_const_buffer_t  input,
    const char         *output_port_name,
    apg_buffer_t        output,
    uint32_t            frames
) {
    if (!host || !host->runtime_ready)
        return false;
    return host_runtime_process_mono_ports(
        &host->registry, host->runtime, input_port_name, input, output_port_name, output, frames
    );
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
    const char *path, const apg_prepare_context_t *prepare_context, apg_v2_host_project_t **out, uc_error *err
) {
    if (!path || !out || !err)
        return UC_E_TYPE;
    *out        = NULL;
    err->status = UC_OK;
    if (!apg_prepare_context_valid(prepare_context))
        return set_error(err, UC_E_RANGE, "v2 host prepare context is invalid");

    apg_v2_host_project_t *host = calloc(1u, sizeof(*host));
    if (!host)
        return set_error(err, UC_E_OOM, "v2 host allocation failed");
    host->prepare_context = *prepare_context;

    if (!host_project_alloc_crossfade_buffers(host)) {
        apg_v2_host_project_destroy(host);
        return set_error(err, UC_E_OOM, "v2 host crossfade buffer allocation failed");
    }

    uc_arena resolved_arena = {0};
    if (uc_arena_init(&resolved_arena, 2 * 1024 * 1024u) != 0) {
        apg_v2_host_project_destroy(host);
        return set_error(err, UC_E_OOM, "v2 host arena allocation failed");
    }

    apg_project_v2_resolved_t resolved_project = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(path, &resolved_arena, &resolved_project, err);
    if (status == UC_OK)
        status = host_project_bundle_create(&resolved_project, prepare_context, &host->active, err);
    uc_arena_free(&resolved_arena);
    if (status != UC_OK) {
        apg_v2_host_project_destroy(host);
        return status;
    }

    *out = host;
    return UC_OK;
}

bool apg_v2_host_project_set_param(apg_v2_host_project_t *host, const char *name, float value) {
    if (!host || !host->active || !host->active->runtime_ready)
        return false;
    if (!host_runtime_set_param(&host->active->registry, host->active->runtime, name, value))
        return false;
    return host_project_shadow_param(host, name, value);
}

bool apg_v2_host_project_set_bypass(apg_v2_host_project_t *host, const char *instance_id, bool enabled) {
    if (!host || !host->active || !host->active->runtime_ready)
        return false;
    if (!host_runtime_set_bypass(&host->active->registry, host->active->runtime, instance_id, enabled))
        return false;
    return host_project_shadow_bypass(host, instance_id, enabled);
}

bool apg_v2_host_project_set_mute(apg_v2_host_project_t *host, bool muted) {
    if (!host || !host->active || !host->active->runtime_ready)
        return false;
    if (!apg_v2_runtime_set_project_mute(host->active->runtime, muted))
        return false;
    host->project_muted = muted;
    return true;
}

const char *apg_v2_host_project_last_error(const apg_v2_host_project_t *host) {
    return host && host->active ? apg_v2_measure_last_error(host->active->runtime) : NULL;
}

uc_status apg_v2_host_project_prepare_swap(
    apg_v2_host_project_t           *host,
    const apg_project_v2_resolved_t *project,
    apg_v2_host_project_swap_t     **out,
    uc_error                        *err
) {
    if (!host || !project || !out || !err)
        return UC_E_TYPE;
    *out        = NULL;
    err->status = UC_OK;

    apg_v2_host_project_swap_t *swap = calloc(1u, sizeof(*swap));
    if (!swap)
        return set_error(err, UC_E_OOM, "v2 host project swap allocation failed");

    uc_status status = host_project_bundle_create(project, &host->prepare_context, &swap->bundle, err);
    if (status != UC_OK) {
        apg_v2_host_project_swap_destroy(&swap);
        return status;
    }

    host_project_apply_shadows(host, swap->bundle);
    *out = swap;
    return UC_OK;
}

bool apg_v2_host_project_commit_swap(apg_v2_host_project_t *host, apg_v2_host_project_swap_t **swap) {
    if (!host || !swap || !*swap || !(*swap)->bundle || !(*swap)->bundle->runtime_ready)
        return false;

    host_project_apply_shadows(host, (*swap)->bundle);
    host_project_bundle_destroy(&host->fade_out);
    host->fade_out  = host->active;
    host->active    = (*swap)->bundle;
    (*swap)->bundle = NULL;
    apg_v2_host_project_swap_destroy(swap);

    host->crossfade_total_frames     = host->prepare_context.maximum_frames < APG_V2_HOST_SWAP_CROSSFADE_FRAMES
                                           ? host->prepare_context.maximum_frames
                                           : APG_V2_HOST_SWAP_CROSSFADE_FRAMES;
    host->crossfade_remaining_frames = host->crossfade_total_frames;
    host->crossfade_offset_frames    = 0u;
    if (host->crossfade_total_frames == 0u)
        host_project_bundle_destroy(&host->fade_out);
    return true;
}

void apg_v2_host_project_swap_destroy(apg_v2_host_project_swap_t **swap) {
    if (!swap || !*swap)
        return;
    host_project_bundle_destroy(&(*swap)->bundle);
    free(*swap);
    *swap = NULL;
}

bool apg_v2_host_project_process_mono_ports(
    apg_v2_host_project_t *host,
    const char            *input_port_name,
    apg_const_buffer_t     input,
    const char            *output_port_name,
    apg_buffer_t           output,
    uint32_t               frames
) {
    if (!host || !host->active || !host->active->runtime_ready || frames > host->prepare_context.maximum_frames ||
        !apg_const_buffer_has_length(input, frames) || !apg_buffer_has_capacity(output, frames))
        return false;
    if (!host->fade_out || host->crossfade_remaining_frames == 0u)
        return host_runtime_process_mono_ports(
            &host->active->registry, host->active->runtime, input_port_name, input, output_port_name, output, frames
        );

    if (!host->crossfade_old || !host->crossfade_new)
        return false;
    if (!host_runtime_process_mono_ports(
            &host->fade_out->registry, host->fade_out->runtime, input_port_name, input, output_port_name,
            apg_buffer_make(host->crossfade_old, host->prepare_context.maximum_frames), frames
        ) ||
        !host_runtime_process_mono_ports(
            &host->active->registry, host->active->runtime, input_port_name, input, output_port_name,
            apg_buffer_make(host->crossfade_new, host->prepare_context.maximum_frames), frames
        )) {
        return false;
    }

    uint32_t mixed_frames = frames < host->crossfade_remaining_frames ? frames : host->crossfade_remaining_frames;
    for (uint32_t i = 0; i < frames; i++) {
        if (i < mixed_frames) {
            float position = (float)(host->crossfade_offset_frames + i + 1u) / (float)host->crossfade_total_frames;
            if (position > 1.0f)
                position = 1.0f;
            output.data[i] = (host->crossfade_old[i] * (1.0f - position)) + (host->crossfade_new[i] * position);
        } else {
            output.data[i] = host->crossfade_new[i];
        }
    }

    host->crossfade_offset_frames += mixed_frames;
    host->crossfade_remaining_frames -= mixed_frames;
    if (host->crossfade_remaining_frames == 0u)
        host_project_bundle_destroy(&host->fade_out);
    return true;
}

void apg_v2_host_project_destroy(apg_v2_host_project_t *host) {
    if (!host)
        return;
    host_project_bundle_destroy(&host->fade_out);
    host_project_bundle_destroy(&host->active);
    host_project_free_shadows(host);
    free(host->crossfade_old);
    free(host->crossfade_new);
    free(host);
}
