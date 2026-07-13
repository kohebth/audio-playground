#include <apg/wasm/abi.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *workspace_paths[] = {
    "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml",
    "test/fixtures/units-v2/noise_gate.unit.v2.yaml",
    "test/fixtures/units-v2/overdrive.unit.v2.yaml",
    "test/fixtures/units-v2/tone_stack.unit.v2.yaml",
    "test/fixtures/units-v2/tremolo.unit.v2.yaml",
    "test/fixtures/units-v2/delay.unit.v2.yaml",
    "test/fixtures/units-v2/wet_dry_mix.unit.v2.yaml",
};

static int fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static int processor_fail(const apg_wasm_processor_t *processor, const char *message) {
    const apg_wasm_diagnostic_t *diagnostic = apg_wasm_processor_last_diagnostic(processor);
    fprintf(
        stderr, "FAIL: %s: %s (%s %s)\n", message, diagnostic && diagnostic->message ? diagnostic->message : "",
        diagnostic && diagnostic->phase ? diagnostic->phase : "", diagnostic && diagnostic->code ? diagnostic->code : ""
    );
    return 1;
}

static char *read_file(const char *relative_path, size_t *out_len) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", APG_TEST_ROOT, relative_path);
    FILE *file = fopen(full_path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0)
        return NULL;
    const long length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *content = malloc((size_t)length + 1u);
    if (!content) {
        fclose(file);
        return NULL;
    }
    const size_t read = fread(content, 1u, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) {
        free(content);
        return NULL;
    }
    content[read] = '\0';
    *out_len      = read;
    return content;
}

static int prepare_revision(apg_wasm_control_t *control, uint64_t revision) {
    const char *entry = workspace_paths[0];
    if (apg_wasm_control_begin_workspace(control, revision, entry, strlen(entry)) != APG_WASM_STATUS_OK)
        return fail("cannot begin processor test workspace");
    for (size_t i = 0u; i < sizeof(workspace_paths) / sizeof(workspace_paths[0]); ++i) {
        size_t content_len = 0u;
        char  *content     = read_file(workspace_paths[i], &content_len);
        if (!content)
            return fail("cannot read processor test fixture");
        const apg_wasm_status_t status = apg_wasm_control_put_file(
            control, i == 0u ? APG_WASM_FILE_PROJECT : APG_WASM_FILE_UNIT, workspace_paths[i],
            strlen(workspace_paths[i]), content, content_len
        );
        free(content);
        if (status != APG_WASM_STATUS_OK)
            return fail("cannot populate processor test workspace");
    }
    const apg_wasm_audio_config_t config = {.revision = revision, .sample_rate = 48000u, .block_frames = 64u};
    return apg_wasm_control_prepare_workspace(control, &config) == APG_WASM_STATUS_OK
               ? 0
               : fail("cannot prepare processor test image");
}

static int output_is_finite(const float *output, uint32_t frames) {
    if (!output)
        return 0;
    for (uint32_t i = 0u; i < frames; ++i) {
        if (!isfinite(output[i]))
            return 0;
    }
    return 1;
}

int main(void) {
    apg_wasm_control_t   *control   = apg_wasm_control_create(0u);
    apg_wasm_processor_t *processor = apg_wasm_processor_create();
    if (!control || !processor)
        return fail("cannot create control or processor");
    if (prepare_revision(control, 10u))
        return 1;

    size_t               image_size = 0u;
    const unsigned char *image      = apg_wasm_control_prepared_image(control, &image_size);
    if (!image || image_size == 0u)
        return fail("prepared image is empty");
    if (apg_wasm_control_param_count(control) == 0u || !apg_wasm_control_param_name(control, 0u) ||
        apg_wasm_control_bypass_count(control) != 6u || !apg_wasm_control_bypass_name(control, 0u))
        return fail("prepared runtime names are incomplete");
    unsigned char *corrupt = malloc(image_size);
    if (!corrupt)
        return fail("cannot allocate corrupt image copy");
    memcpy(corrupt, image, image_size);
    corrupt[image_size - 1u] ^= 0x5au;
    if (apg_wasm_processor_stage_image(processor, corrupt, image_size) != APG_WASM_STATUS_INCOMPATIBLE_IMAGE)
        return fail("corrupt image was accepted");
    free(corrupt);
    if (apg_wasm_processor_active_revision(processor) != 0u)
        return fail("corrupt image changed active revision");

    if (apg_wasm_processor_stage_image(processor, image, image_size) != APG_WASM_STATUS_OK)
        return processor_fail(processor, "valid image could not be staged");
    if (apg_wasm_processor_commit_staged(processor, 9u) != APG_WASM_STATUS_STALE_REVISION)
        return fail("mismatched image revision was committed");
    if (apg_wasm_processor_commit_staged(processor, 10u) != APG_WASM_STATUS_OK)
        return fail("valid image could not be committed");
    if (apg_wasm_processor_frame_capacity(processor) != 64u)
        return fail("processor frame capacity is incorrect");

    float *input = apg_wasm_processor_input_buffer(processor);
    for (uint32_t i = 0u; i < 64u; ++i)
        input[i] = 0.25f * sinf((float)i * 0.1f);
    if (apg_wasm_processor_process(processor, 64u) != APG_WASM_STATUS_OK)
        return fail("initial processor block failed");
    if (apg_wasm_processor_active_revision(processor) != 10u ||
        !output_is_finite(apg_wasm_processor_output_buffer(processor), 64u))
        return fail("initial runtime output is invalid");

    const apg_wasm_meter_snapshot_t *meter = apg_wasm_processor_output_meter(processor);
    if (!meter || !meter->valid || meter->frames != 64u || !isfinite(meter->peak) || !isfinite(meter->rms))
        return fail("processor output meter is invalid");
    if (apg_wasm_processor_set_param(processor, 0u, 0.2f) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_set_bypass(processor, 0u, 1u) != APG_WASM_STATUS_OK)
        return fail("processor controls failed");
    if (apg_wasm_processor_set_mute(processor, 1u) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_process(processor, 64u) != APG_WASM_STATUS_OK)
        return fail("processor mute failed");
    const float *output = apg_wasm_processor_output_buffer(processor);
    for (uint32_t i = 0u; i < 64u; ++i) {
        if (output[i] != 0.0f)
            return fail("muted processor emitted audio");
    }
    if (apg_wasm_processor_set_mute(processor, 0u) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_reset(processor) != APG_WASM_STATUS_OK)
        return fail("processor unmute or reset failed");

    if (prepare_revision(control, 11u))
        return 1;
    image = apg_wasm_control_prepared_image(control, &image_size);
    if (apg_wasm_processor_stage_image(processor, image, image_size) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_commit_staged(processor, 11u) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_process(processor, 64u) != APG_WASM_STATUS_OK)
        return fail("replacement runtime swap failed");
    if (apg_wasm_processor_active_revision(processor) != 11u ||
        !output_is_finite(apg_wasm_processor_output_buffer(processor), 64u))
        return fail("replacement runtime output is invalid");

    image   = apg_wasm_control_prepared_image(control, &image_size);
    corrupt = malloc(image_size);
    if (!corrupt)
        return fail("cannot allocate post-swap corrupt image copy");
    memcpy(corrupt, image, image_size);
    corrupt[32u] ^= 0x1u;
    if (apg_wasm_processor_stage_image(processor, corrupt, image_size) != APG_WASM_STATUS_INCOMPATIBLE_IMAGE ||
        apg_wasm_processor_active_revision(processor) != 11u)
        return fail("invalid replacement disturbed active runtime");
    free(corrupt);

    if (prepare_revision(control, 12u))
        return 1;
    image = apg_wasm_control_prepared_image(control, &image_size);
    if (apg_wasm_processor_stage_image(processor, image, image_size) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_commit_staged(processor, 12u) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_process(processor, 64u) != APG_WASM_STATUS_OK ||
        apg_wasm_processor_active_revision(processor) != 12u)
        return fail("retired runtime cleanup blocked a later swap");

    apg_wasm_processor_destroy(processor);
    apg_wasm_control_destroy(control);
    return 0;
}
