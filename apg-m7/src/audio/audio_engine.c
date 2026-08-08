#include "apg_m7/domain/audio_engine.h"
#include "apgcore/runtime/runtime_v2.h"
#include <string.h>

static volatile apg_m7_engine_state_t g_engine_state = APG_M7_ENGINE_UNINITIALIZED;
static apg_m7_engine_stats_t g_engine_stats = {0};
static const apg_m7_audio_driver_t *g_active_driver = NULL;
static apg_v2_runtime_t * volatile g_runtime = NULL;

void apg_m7_audio_engine_init_with_driver(const apg_m7_audio_driver_t *driver) {
    memset(&g_engine_stats, 0, sizeof(g_engine_stats));
    g_active_driver = driver;
    if (g_active_driver && g_active_driver->init) {
        g_active_driver->init(APG_M7_AUDIO_SAMPLE_RATE_HZ);
    }
    g_engine_state = APG_M7_ENGINE_READY;
}

void apg_m7_audio_engine_init(void) {
#if APG_M7_SELECTED_AUDIO_IO == 1
    apg_m7_audio_engine_init_with_driver(apg_m7_audio_driver_get_gpio_adc_dac());
#else
    apg_m7_audio_engine_init_with_driver(apg_m7_audio_driver_get_wm8960());
#endif
}

#include "apgcore/parser/parser_v2.h"
#include "yaml/arena.h"
#include "yaml/error.h"

bool apg_m7_audio_engine_load_yaml_preset(const char *yaml_content, size_t yaml_len) {
    if (!yaml_content || yaml_len == 0) {
        return false;
    }
    
    uc_arena arena;
    if (uc_arena_init(&arena, 64 * 1024) != 0) {
        return false;
    }

    uc_node *root = NULL;
    uc_error err;
    memset(&err, 0, sizeof(err));

    uc_status status = apg_v2_parse_string(yaml_content, yaml_len, &arena, &root, &err);
    if (status != UC_OK || !root) {
        uc_arena_free(&arena);
        return false;
    }

    /* Valid project v2 YAML syntax parsed and validated into AST node */
    uc_arena_free(&arena);
    return true;
}

void apg_m7_audio_engine_start(void) {
    if (g_engine_state == APG_M7_ENGINE_READY) {
        g_engine_state = APG_M7_ENGINE_RUNNING;
    }
}

void apg_m7_audio_engine_stop(void) {
    if (g_engine_state == APG_M7_ENGINE_RUNNING) {
        g_engine_state = APG_M7_ENGINE_READY;
    }
}

#if defined(__ARM_ARCH_7EM__) || defined(__arm__)
__attribute__((section(".itcm")))
#endif
void apg_m7_audio_dma_callback(const float *input_stereo, float *output_stereo, uint32_t frames) {
    if (g_engine_state != APG_M7_ENGINE_RUNNING || !input_stereo || !output_stereo) {
        if (output_stereo) {
            memset(output_stereo, 0, frames * 2 * sizeof(float));
        }
        return;
    }

    bool processed = false;
    apg_v2_runtime_t *rt = g_runtime;
    if (rt) {
        apg_const_buffer_t in_buf = apg_const_buffer_make(input_stereo, frames * 2);
        apg_buffer_t out_buf = apg_buffer_make(output_stereo, frames * 2);
        processed = apg_v2_runtime_process_interleaved_port_indices(rt, 0, in_buf, 0, out_buf, frames);
    }

    if (!processed) {
        /* Fallback passthrough processing when runtime is unassigned */
        for (uint32_t i = 0; i < frames * 2; ++i) {
            output_stereo[i] = input_stereo[i];
        }
    }
    g_engine_stats.processed_blocks++;
}

apg_m7_engine_state_t apg_m7_audio_engine_get_state(void) {
    return g_engine_state;
}

void apg_m7_audio_engine_get_stats(apg_m7_engine_stats_t *out_stats) {
    if (out_stats) {
        *out_stats = g_engine_stats;
    }
}
