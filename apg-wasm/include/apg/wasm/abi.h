#ifndef AUDIO_PLAYGROUND_WASM_TOOLS_ABI_H
#define AUDIO_PLAYGROUND_WASM_TOOLS_ABI_H

#include <stddef.h>
#include <stdint.h>

#define APG_WASM_ABI_VERSION 1u

typedef enum {
    APG_WASM_STATUS_OK = 0,
    APG_WASM_STATUS_INVALID_ARGUMENT,
    APG_WASM_STATUS_STALE_REVISION,
    APG_WASM_STATUS_PARSE_ERROR,
    APG_WASM_STATUS_VALIDATION_ERROR,
    APG_WASM_STATUS_COMPILE_ERROR,
    APG_WASM_STATUS_INCOMPATIBLE_IMAGE,
    APG_WASM_STATUS_RUNTIME_ERROR,
    APG_WASM_STATUS_OUT_OF_MEMORY,
} apg_wasm_status_t;

typedef enum {
    APG_WASM_FILE_PROJECT = 1,
    APG_WASM_FILE_UNIT    = 2,
} apg_wasm_file_role_t;

typedef enum {
    APG_WASM_CAP_NONE           = 0,
    APG_WASM_CAP_WORKSPACE      = 1u << 0,
    APG_WASM_CAP_PREPARED_IMAGE = 1u << 1,
    APG_WASM_CAP_PROCESS        = 1u << 2,
    APG_WASM_CAP_CONTROLS       = 1u << 3,
    APG_WASM_CAP_METERS         = 1u << 4,
} apg_wasm_capability_t;

typedef struct {
    uint64_t revision;
    uint32_t sample_rate;
    uint32_t block_frames;
} apg_wasm_audio_config_t;

typedef struct {
    uint64_t          revision;
    apg_wasm_status_t status;
    const char       *phase;
    const char       *code;
    const char       *file;
    const char       *path;
    const char       *message;
} apg_wasm_diagnostic_t;

typedef struct apg_wasm_control   apg_wasm_control_t;
typedef struct apg_wasm_processor apg_wasm_processor_t;

typedef struct {
    uint64_t revision;
    uint32_t unit_count;
    uint32_t instance_count;
    uint32_t node_count;
    uint32_t schedule_count;
    uint32_t signal_count;
    uint32_t param_count;
} apg_wasm_workspace_summary_t;

typedef struct {
    float    peak;
    float    rms;
    uint32_t frames;
    uint32_t valid;
} apg_wasm_meter_snapshot_t;

uint32_t            apg_wasm_control_abi_version(void);
uint32_t            apg_wasm_control_capabilities(void);
apg_wasm_control_t *apg_wasm_control_create(size_t arena_bytes);
void                apg_wasm_control_destroy(apg_wasm_control_t *control);
apg_wasm_status_t   apg_wasm_control_begin_workspace(
      apg_wasm_control_t *control, uint64_t revision, const char *entry_project, size_t entry_project_len
  );
apg_wasm_status_t apg_wasm_control_put_file(
    apg_wasm_control_t  *control,
    apg_wasm_file_role_t role,
    const char          *path,
    size_t               path_len,
    const char          *content,
    size_t               content_len
);
apg_wasm_status_t apg_wasm_control_validate_workspace(apg_wasm_control_t *control);
apg_wasm_status_t apg_wasm_control_compile_workspace(apg_wasm_control_t *control);
apg_wasm_status_t
apg_wasm_control_prepare_workspace(apg_wasm_control_t *control, const apg_wasm_audio_config_t *config);
const unsigned char         *apg_wasm_control_prepared_image(const apg_wasm_control_t *control, size_t *out_size);
uint32_t                     apg_wasm_control_param_count(const apg_wasm_control_t *control);
const char                  *apg_wasm_control_param_name(const apg_wasm_control_t *control, uint32_t index);
uint32_t                     apg_wasm_control_bypass_count(const apg_wasm_control_t *control);
const char                  *apg_wasm_control_bypass_name(const apg_wasm_control_t *control, uint32_t index);
const apg_wasm_diagnostic_t *apg_wasm_control_last_diagnostic(const apg_wasm_control_t *control);
const apg_wasm_workspace_summary_t *apg_wasm_control_workspace_summary(const apg_wasm_control_t *control);
uint32_t                            apg_wasm_processor_abi_version(void);
uint32_t                            apg_wasm_processor_capabilities(void);
apg_wasm_processor_t               *apg_wasm_processor_create(void);
void                                apg_wasm_processor_destroy(apg_wasm_processor_t *processor);
apg_wasm_status_t
apg_wasm_processor_stage_image(apg_wasm_processor_t *processor, const unsigned char *image, size_t image_size);
apg_wasm_status_t apg_wasm_processor_commit_staged(apg_wasm_processor_t *processor, uint64_t revision);
float            *apg_wasm_processor_input_buffer(apg_wasm_processor_t *processor);
const float      *apg_wasm_processor_output_buffer(const apg_wasm_processor_t *processor);
uint32_t          apg_wasm_processor_frame_capacity(const apg_wasm_processor_t *processor);
uint64_t          apg_wasm_processor_active_revision(const apg_wasm_processor_t *processor);
apg_wasm_status_t apg_wasm_processor_process(apg_wasm_processor_t *processor, uint32_t frames);
apg_wasm_status_t apg_wasm_processor_set_param(apg_wasm_processor_t *processor, uint32_t index, float value);
apg_wasm_status_t apg_wasm_processor_set_bypass(apg_wasm_processor_t *processor, uint32_t index, uint32_t enabled);
apg_wasm_status_t apg_wasm_processor_set_mute(apg_wasm_processor_t *processor, uint32_t enabled);
apg_wasm_status_t apg_wasm_processor_reset(apg_wasm_processor_t *processor);
const apg_wasm_meter_snapshot_t *apg_wasm_processor_output_meter(apg_wasm_processor_t *processor);
const apg_wasm_diagnostic_t     *apg_wasm_processor_last_diagnostic(const apg_wasm_processor_t *processor);

#endif // AUDIO_PLAYGROUND_WASM_TOOLS_ABI_H
