#include <apgcore/host/host_v2.h>
#include <apgcore/measure/measure_v2.h>
#include <apgcore/parser/parser_v2.h>
#include <apgcore/runtime/buffer.h>
#include <apgcore/runtime/prepare.h>
#include <apgcore/runtime/process.h>
#include <apgcore/runtime/runtime_v2.h>
#include <apgcore/runtime/spectral.h>
#include <apgcore/runtime/stream.h>
#include <apgcore/validator/project_v2.h>
#include <apgcore/validator/unit_v2.h>

int main(void) {
    apg_v2_host_unit_t    *unit_host    = 0;
    apg_v2_host_project_t *project_host = 0;
    apg_v2_runtime_t      *runtime      = 0;
    float                  samples[64]  = {0};
    apg_buffer_t           buffer       = apg_buffer_make(samples, 64u);
    apg_const_buffer_t     const_buffer = apg_const_buffer_make(samples, 64u);
    apg_prepare_context_t  prepare      = {.maximum_frames = 64u, .sample_rate = 48000.0f};
    apg_process_context_t  context      = {.frames = 64u, .sample_rate = 48000.0f, .sample_position = 0u};
    apg_spectral_info_t    spectral     = {.fft_size = 256u, .bin_count = 129u, .hop_size = 256u};
    apg_stream_context_t   stream       = {
                .input_frames = 64u, .output_capacity = 128u, .sample_rate = 48000.0f, .sample_position = 0u
    };
    apg_stream_result_t result = apg_stream_result_empty();

    return unit_host || project_host || runtime || !apg_buffer_has_capacity(buffer, 64u) ||
                   !apg_const_buffer_has_length(const_buffer, 64u) || !apg_prepare_context_valid(&prepare) ||
                   !apg_process_context_valid(&context) || !apg_spectral_info_valid(&spectral) ||
                   !apg_stream_context_valid(&stream) || result.consumed_frames != 0u || result.produced_frames != 0u
               ? 1
               : 0;
}
