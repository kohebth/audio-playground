#include <apg/wasm/abi.h>

#include <apgcore/measure/measure_v2.h>
#include <apgcore/runtime/runtime_v2.h>
#include <yaml/arena.h>
#include <yaml/error.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#define APG_WASM_PROCESSOR_ARENA_MIN       (64u * 1024u)
#define APG_WASM_PROCESSOR_ARENA_SCALE     16u
#define APG_WASM_PROCESSOR_DIAGNOSTIC_TEXT 128u

typedef struct {
    uc_arena          arena;
    apg_v2_registry_t registry;
    apg_v2_runtime_t *runtime;
    uint64_t          revision;
    bool              ready;
} apg_wasm_processor_slot_t;

struct apg_wasm_processor {
    apg_wasm_processor_slot_t active;
    apg_wasm_processor_slot_t staged;
    apg_wasm_processor_slot_t retired;
    bool                      commit_pending;
    float                    *input;
    float                    *output;
    float                    *crossfade;
    uint32_t                  buffer_capacity;
    apg_wasm_meter_snapshot_t meter;
    apg_wasm_diagnostic_t     diagnostic;
    char                      diagnostic_phase[32];
    char                      diagnostic_code[32];
    char                      diagnostic_message[APG_WASM_PROCESSOR_DIAGNOSTIC_TEXT];
};

static void slot_destroy(apg_wasm_processor_slot_t *slot) {
    if (!slot)
        return;
    apg_v2_runtime_destroy_owned(&slot->runtime);
    uc_arena_free(&slot->arena);
    memset(slot, 0, sizeof(*slot));
}

static apg_wasm_status_t set_diagnostic(
    apg_wasm_processor_t *processor, apg_wasm_status_t status, const char *phase, const char *code, const char *message
) {
    if (!processor)
        return status;
    snprintf(processor->diagnostic_phase, sizeof(processor->diagnostic_phase), "%s", phase ? phase : "");
    snprintf(processor->diagnostic_code, sizeof(processor->diagnostic_code), "%s", code ? code : "");
    snprintf(processor->diagnostic_message, sizeof(processor->diagnostic_message), "%s", message ? message : "");
    processor->diagnostic = (apg_wasm_diagnostic_t){
        .revision = processor->staged.ready ? processor->staged.revision : processor->active.revision,
        .status   = status,
        .phase    = processor->diagnostic_phase,
        .code     = processor->diagnostic_code,
        .file     = "",
        .path     = "",
        .message  = processor->diagnostic_message,
    };
    return status;
}

static bool ensure_buffers(apg_wasm_processor_t *processor, uint32_t capacity) {
    if (capacity <= processor->buffer_capacity)
        return true;
    if ((size_t)capacity > SIZE_MAX / sizeof(float))
        return false;
    float *input = realloc(processor->input, (size_t)capacity * sizeof(*input));
    if (!input)
        return false;
    processor->input = input;
    float *output    = realloc(processor->output, (size_t)capacity * sizeof(*output));
    if (!output)
        return false;
    processor->output = output;
    float *crossfade  = realloc(processor->crossfade, (size_t)capacity * sizeof(*crossfade));
    if (!crossfade)
        return false;
    processor->crossfade       = crossfade;
    processor->buffer_capacity = capacity;
    memset(processor->input, 0, (size_t)capacity * sizeof(*processor->input));
    memset(processor->output, 0, (size_t)capacity * sizeof(*processor->output));
    memset(processor->crossfade, 0, (size_t)capacity * sizeof(*processor->crossfade));
    return true;
}

static apg_wasm_status_t
runtime_failure(apg_wasm_processor_t *processor, uint64_t revision, const char *phase, const char *fallback) {
    if (!processor)
        return APG_WASM_STATUS_RUNTIME_ERROR;
    processor->diagnostic = (apg_wasm_diagnostic_t){
        .revision = revision,
        .status   = APG_WASM_STATUS_RUNTIME_ERROR,
        .phase    = phase,
        .code     = "APG_RUNTIME_ERROR",
        .file     = "",
        .path     = "",
        .message  = fallback,
    };
    return APG_WASM_STATUS_RUNTIME_ERROR;
}

uint32_t apg_wasm_processor_abi_version(void) { return APG_WASM_ABI_VERSION; }

uint32_t apg_wasm_processor_capabilities(void) {
    return APG_WASM_CAP_PREPARED_IMAGE | APG_WASM_CAP_PROCESS | APG_WASM_CAP_CONTROLS | APG_WASM_CAP_METERS;
}

apg_wasm_processor_t *apg_wasm_processor_create(void) {
    apg_wasm_processor_t *processor = calloc(1u, sizeof(*processor));
    if (processor)
        set_diagnostic(processor, APG_WASM_STATUS_OK, "idle", "APG_OK", "");
    return processor;
}

void apg_wasm_processor_destroy(apg_wasm_processor_t *processor) {
    if (!processor)
        return;
    slot_destroy(&processor->active);
    slot_destroy(&processor->staged);
    slot_destroy(&processor->retired);
    free(processor->input);
    free(processor->output);
    free(processor->crossfade);
    free(processor);
}

apg_wasm_status_t
apg_wasm_processor_stage_image(apg_wasm_processor_t *processor, const unsigned char *image, size_t image_size) {
    if (!processor || !image || image_size == 0u)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    if (image_size > (SIZE_MAX - APG_WASM_PROCESSOR_ARENA_MIN) / APG_WASM_PROCESSOR_ARENA_SCALE)
        return set_diagnostic(
            processor, APG_WASM_STATUS_OUT_OF_MEMORY, "stage", "APG_IMAGE_TOO_LARGE", "prepared image is too large"
        );

    slot_destroy(&processor->retired);

    apg_wasm_processor_slot_t staged     = {0};
    const size_t              arena_size = APG_WASM_PROCESSOR_ARENA_MIN + image_size * APG_WASM_PROCESSOR_ARENA_SCALE;
    if (uc_arena_init(&staged.arena, arena_size) != 0)
        return set_diagnostic(
            processor, APG_WASM_STATUS_OUT_OF_MEMORY, "stage", "APG_OOM", "cannot allocate registry arena"
        );

    uc_error error = {0};
    if (!apg_wasm_image_hydrate(image, image_size, &staged.arena, &staged.registry, &staged.revision, &error)) {
        uc_arena_free(&staged.arena);
        return set_diagnostic(processor, APG_WASM_STATUS_INCOMPATIBLE_IMAGE, "stage", "APG_IMAGE_INVALID", error.msg);
    }
    if (processor->active.ready && (staged.registry.frame_capacity != processor->active.registry.frame_capacity ||
                                    staged.registry.sample_rate != processor->active.registry.sample_rate)) {
        uc_arena_free(&staged.arena);
        return set_diagnostic(
            processor, APG_WASM_STATUS_INCOMPATIBLE_IMAGE, "stage", "APG_AUDIO_CONFIG_MISMATCH",
            "replacement audio configuration differs from the active runtime"
        );
    }
    if (apg_v2_runtime_create_from_registry(&staged.registry, &staged.runtime, &error) != UC_OK) {
        uc_arena_free(&staged.arena);
        return set_diagnostic(
            processor, error.status == UC_E_OOM ? APG_WASM_STATUS_OUT_OF_MEMORY : APG_WASM_STATUS_RUNTIME_ERROR,
            "stage", uc_status_str(error.status), error.msg
        );
    }
    staged.ready = true;
    if (!ensure_buffers(processor, staged.registry.frame_capacity)) {
        slot_destroy(&staged);
        return set_diagnostic(
            processor, APG_WASM_STATUS_OUT_OF_MEMORY, "stage", "APG_OOM", "cannot allocate audio buffers"
        );
    }

    slot_destroy(&processor->staged);
    processor->staged         = staged;
    processor->commit_pending = false;
    return set_diagnostic(processor, APG_WASM_STATUS_OK, "stage", "APG_OK", "");
}

apg_wasm_status_t apg_wasm_processor_commit_staged(apg_wasm_processor_t *processor, uint64_t revision) {
    if (!processor || !processor->staged.ready || processor->staged.revision != revision)
        return set_diagnostic(
            processor, APG_WASM_STATUS_STALE_REVISION, "commit", "APG_STALE_REVISION",
            "staged image revision does not match commit revision"
        );
    processor->commit_pending = true;
    return set_diagnostic(processor, APG_WASM_STATUS_OK, "commit", "APG_OK", "");
}

float *apg_wasm_processor_input_buffer(apg_wasm_processor_t *processor) { return processor ? processor->input : NULL; }

const float *apg_wasm_processor_output_buffer(const apg_wasm_processor_t *processor) {
    return processor ? processor->output : NULL;
}

uint32_t apg_wasm_processor_frame_capacity(const apg_wasm_processor_t *processor) {
    return processor ? processor->buffer_capacity : 0u;
}

uint64_t apg_wasm_processor_active_revision(const apg_wasm_processor_t *processor) {
    return processor && processor->active.ready ? processor->active.revision : 0u;
}

static bool process_slot(apg_wasm_processor_slot_t *slot, const float *input, float *output, uint32_t frames) {
    return slot && slot->ready && slot->runtime &&
           apg_v2_runtime_process_mono(
               slot->runtime, apg_const_buffer_make(input, frames), apg_buffer_make(output, frames), frames
           );
}

apg_wasm_status_t apg_wasm_processor_process(apg_wasm_processor_t *processor, uint32_t frames) {
    if (!processor || !processor->input || !processor->output || frames == 0u || frames > processor->buffer_capacity)
        return APG_WASM_STATUS_INVALID_ARGUMENT;

    if (processor->commit_pending) {
        if (!processor->active.ready) {
            if (!process_slot(&processor->staged, processor->input, processor->output, frames))
                return runtime_failure(
                    processor, processor->staged.revision, "process", "staged runtime processing failed"
                );
            processor->active = processor->staged;
            memset(&processor->staged, 0, sizeof(processor->staged));
        } else {
            if (!process_slot(&processor->active, processor->input, processor->crossfade, frames))
                return runtime_failure(
                    processor, processor->active.revision, "process", "active runtime processing failed"
                );
            if (!process_slot(&processor->staged, processor->input, processor->output, frames)) {
                const uint64_t failed_revision = processor->staged.revision;
                memcpy(processor->output, processor->crossfade, (size_t)frames * sizeof(*processor->output));
                processor->retired = processor->staged;
                memset(&processor->staged, 0, sizeof(processor->staged));
                processor->commit_pending = false;
                return runtime_failure(processor, failed_revision, "process", "staged runtime processing failed");
            }
            for (uint32_t i = 0u; i < frames; ++i) {
                const float mix      = frames == 1u ? 1.0f : (float)i / (float)(frames - 1u);
                processor->output[i] = processor->crossfade[i] * (1.0f - mix) + processor->output[i] * mix;
            }
            processor->retired = processor->active;
            processor->active  = processor->staged;
            memset(&processor->staged, 0, sizeof(processor->staged));
        }
        processor->commit_pending = false;
    } else if (!process_slot(&processor->active, processor->input, processor->output, frames)) {
        return runtime_failure(processor, processor->active.revision, "process", "no active runtime is available");
    }

    return APG_WASM_STATUS_OK;
}

apg_wasm_status_t apg_wasm_processor_set_param(apg_wasm_processor_t *processor, uint32_t index, float value) {
    if (!processor || !isfinite(value))
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    bool updated = processor->active.runtime && apg_v2_runtime_set_param_index(processor->active.runtime, index, value);
    if (processor->staged.runtime)
        updated = apg_v2_runtime_set_param_index(processor->staged.runtime, index, value) || updated;
    return updated ? set_diagnostic(processor, APG_WASM_STATUS_OK, "control", "APG_OK", "")
                   : set_diagnostic(
                         processor, APG_WASM_STATUS_INVALID_ARGUMENT, "control", "APG_PARAM_INVALID",
                         "parameter index is invalid"
                     );
}

apg_wasm_status_t apg_wasm_processor_set_bypass(apg_wasm_processor_t *processor, uint32_t index, uint32_t enabled) {
    if (!processor)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    bool updated = processor->active.runtime &&
                   apg_v2_runtime_set_instance_bypass_index(processor->active.runtime, index, enabled != 0u);
    if (processor->staged.runtime)
        updated = apg_v2_runtime_set_instance_bypass_index(processor->staged.runtime, index, enabled != 0u) || updated;
    return updated ? set_diagnostic(processor, APG_WASM_STATUS_OK, "control", "APG_OK", "")
                   : set_diagnostic(
                         processor, APG_WASM_STATUS_INVALID_ARGUMENT, "control", "APG_BYPASS_INVALID",
                         "bypass index is invalid"
                     );
}

apg_wasm_status_t apg_wasm_processor_set_mute(apg_wasm_processor_t *processor, uint32_t enabled) {
    if (!processor)
        return APG_WASM_STATUS_INVALID_ARGUMENT;
    bool updated =
        processor->active.runtime && apg_v2_runtime_set_project_mute(processor->active.runtime, enabled != 0u);
    if (processor->staged.runtime)
        updated = apg_v2_runtime_set_project_mute(processor->staged.runtime, enabled != 0u) || updated;
    return updated ? set_diagnostic(processor, APG_WASM_STATUS_OK, "control", "APG_OK", "")
                   : set_diagnostic(
                         processor, APG_WASM_STATUS_RUNTIME_ERROR, "control", "APG_RUNTIME_MISSING",
                         "no runtime is available"
                     );
}

apg_wasm_status_t apg_wasm_processor_reset(apg_wasm_processor_t *processor) {
    if (!processor || !processor->active.runtime || !apg_v2_runtime_reset(processor->active.runtime))
        return set_diagnostic(
            processor, APG_WASM_STATUS_RUNTIME_ERROR, "control", "APG_RESET_FAILED", "active runtime reset failed"
        );
    return set_diagnostic(processor, APG_WASM_STATUS_OK, "control", "APG_OK", "");
}

const apg_wasm_meter_snapshot_t *apg_wasm_processor_output_meter(apg_wasm_processor_t *processor) {
    if (!processor || !processor->active.runtime || processor->active.registry.output_audio_ports_len == 0u)
        return NULL;
    apg_v2_meter_snapshot_t meter = {0};
    const char             *port  = processor->active.registry.output_audio_ports[0].port_name;
    if (!apg_v2_measure_get_output_meter(processor->active.runtime, port, 0u, &meter))
        return NULL;
    processor->meter = (apg_wasm_meter_snapshot_t
    ){.peak = meter.peak, .rms = meter.rms, .frames = meter.frames, .valid = meter.valid ? 1u : 0u};
    return &processor->meter;
}

const apg_wasm_diagnostic_t *apg_wasm_processor_last_diagnostic(const apg_wasm_processor_t *processor) {
    return processor ? &processor->diagnostic : NULL;
}
