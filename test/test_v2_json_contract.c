#include <apgcore/host/json_contract_v2.h>
#include <apgcore/metadata/atom_catalog.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*json_writer_fn)(FILE *out, const char *path);

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static char *read_stream(FILE *file) {
    long size = ftell(file);
    if (size < 0)
        return NULL;
    rewind(file);
    char *buffer = malloc((size_t)size + 1u);
    if (!buffer)
        return NULL;
    size_t read_len  = fread(buffer, 1u, (size_t)size, file);
    buffer[read_len] = '\0';
    return buffer;
}

static char *capture_json(json_writer_fn writer, const char *path) {
    FILE *file = tmpfile();
    if (!file)
        return NULL;
    writer(file, path);
    char *json = read_stream(file);
    fclose(file);
    return json;
}

static char *capture_atom_catalog(void) {
    FILE *file = tmpfile();
    if (!file)
        return NULL;
    apg_atom_catalog_write_json(file);
    char *json = read_stream(file);
    fclose(file);
    return json;
}

static uint64_t fnv1a64(const char *text) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *p = (const unsigned char *)text; p && *p; p++) {
        hash ^= (uint64_t)*p;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    char *buffer = malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    size_t read_len = fread(buffer, 1u, (size_t)size, file);
    fclose(file);
    buffer[read_len] = '\0';
    return buffer;
}

static int expect_golden(json_writer_fn writer, const char *input_path, const char *golden_path, const char *label) {
    char *actual   = capture_json(writer, input_path);
    char *expected = read_file(golden_path);
    if (!actual || !expected) {
        free(actual);
        free(expected);
        return fail("failed to read json or golden fixture");
    }
    int ok = strcmp(actual, expected) == 0;
    if (!ok)
        fprintf(stderr, "json mismatch for %s\nactual:   %s\nexpected: %s\n", label, actual, expected);
    free(actual);
    free(expected);
    return ok ? 0 : 1;
}

static int test_validate_json_golden_outputs(void) {
    if (expect_golden(
            apg_v2_json_write_validate_unit, "test/fixtures/units-v2/simple_gain.unit.v2.yaml",
            "test/golden/v2-validate-unit-simple_gain.json", "unit validation"
        ))
        return 1;
    if (expect_golden(
            apg_v2_json_write_validate_project, "test/fixtures/projects-v2/two-gain-chain.project.v2.yaml",
            "test/golden/v2-validate-project-two-gain-chain.json", "project validation"
        ))
        return 1;
    return 0;
}

static int test_project_inspect_json_golden_output(void) {
    return expect_golden(
        apg_v2_json_write_inspect_project, "test/fixtures/projects-v2/two-gain-chain.project.v2.yaml",
        "test/golden/v2-inspect-project-two-gain-chain.json", "project inspect"
    );
}

static int test_pedalboard_fixture_golden_outputs(void) {
    if (expect_golden(
            apg_v2_json_write_validate_project, "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml",
            "test/golden/v2-validate-project-guitar-pedalboard.json", "pedalboard project validation"
        ))
        return 1;
    if (expect_golden(
            apg_v2_json_write_inspect_project, "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml",
            "test/golden/v2-inspect-project-guitar-pedalboard.json", "pedalboard project inspect"
        ))
        return 1;
    return expect_golden(
        apg_v2_json_write_render_project, "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml",
        "test/golden/v2-render-project-guitar-pedalboard.json", "pedalboard project render"
    );
}

static int test_project_render_json_is_deterministic(void) {
    char *first =
        capture_json(apg_v2_json_write_render_project, "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml");
    char *second =
        capture_json(apg_v2_json_write_render_project, "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml");
    if (!first || !second) {
        free(first);
        free(second);
        return fail("failed to write project render json");
    }
    if (strcmp(first, second) != 0) {
        fprintf(stderr, "project render json was not deterministic\nfirst:  %s\nsecond: %s\n", first, second);
        free(first);
        free(second);
        return 1;
    }
    if (!strstr(first, "\"schema\":\"apg.project.render.v2\"") || !strstr(first, "\"ok\":true") ||
        !strstr(first, "\"input\":\"deterministic_mono_v1\"") || !strstr(first, "\"frames\":16") ||
        !strstr(first, "\"samples\":[") || strstr(first, "\"peak\":0.000000")) {
        free(first);
        free(second);
        return fail("project render json lacked stable non-empty output fields");
    }
    free(first);
    free(second);
    return 0;
}

static int test_unit_inspect_json_golden_output(void) {
    return expect_golden(
        apg_v2_json_write_inspect_unit, "test/fixtures/units-v2/simple_gain.unit.v2.yaml",
        "test/golden/v2-inspect-unit-simple_gain.json", "unit inspect"
    );
}

static int test_unit_inspect_json_contains_ui_contract(void) {
    char *json = capture_json(apg_v2_json_write_inspect_unit, "test/fixtures/units-v2/simple_gain.unit.v2.yaml");
    if (!json)
        return fail("failed to write unit inspect json");
    if (!strstr(json, "\"schema\":\"apg.unit.inspect.v2\"") || !strstr(json, "\"name\":\"simple_gain\"") ||
        !strstr(json, "\"compatibility\":{\"desktop_full\":true,\"wasm_realtime\":true") ||
        !strstr(json, "\"ui\":{\"label\":\"Gain\",\"control\":\"knob\",\"unit\":\"x\"}") ||
        !strstr(json, "\"nodes\":[{\"id\":\"gain_value\",\"atom\":\"generation_dc\"")) {
        free(json);
        return fail("unit inspect json lacked stable UI or graph fields");
    }
    free(json);
    return 0;
}

static int test_invalid_validation_json_contains_diagnostic_fields(void) {
    char *json = capture_json(
        apg_v2_json_write_validate_project, "test/fixtures/projects-v2/invalid-missing-unit.project.v2.yaml"
    );
    if (!json)
        return fail("failed to write invalid validation json");
    if (!strstr(json, "\"ok\":false") || !strstr(json, "\"errors\":[{") || !strstr(json, "\"code\":\"APG_IO_ERROR\"") ||
        !strstr(json, "\"file\":\"test/fixtures/projects-v2/invalid-missing-unit.project.v2.yaml\"") ||
        !strstr(json, "\"path\":\"$.project\"") || !strstr(json, "cannot resolve unit file")) {
        free(json);
        return fail("invalid validation json lacked stable diagnostic fields");
    }
    free(json);
    return 0;
}

static int test_atom_inspect_json_is_available(void) {
    char *json = capture_atom_catalog();
    if (!json)
        return fail("failed to write atom catalog json");
    if (!strstr(json, "\"schema\":\"apg.atom_catalog.v2\"") || !strstr(json, "\"name\":\"generation_dc\"") ||
        !strstr(json, "\"name\":\"coefficients\",\"type\":\"float_matrix\"")) {
        free(json);
        return fail("atom inspect json lacked expected catalog fields");
    }
    if (strlen(json) != 26851u || fnv1a64(json) != UINT64_C(0x4254a50cf83f2d48)) {
        free(json);
        return fail("atom inspect json changed from the frozen sample contract");
    }
    free(json);
    return 0;
}

int main(void) {
    if (test_validate_json_golden_outputs())
        return 1;
    if (test_project_inspect_json_golden_output())
        return 1;
    if (test_project_render_json_is_deterministic())
        return 1;
    if (test_pedalboard_fixture_golden_outputs())
        return 1;
    if (test_unit_inspect_json_golden_output())
        return 1;
    if (test_unit_inspect_json_contains_ui_contract())
        return 1;
    if (test_invalid_validation_json_contains_diagnostic_fields())
        return 1;
    if (test_atom_inspect_json_is_available())
        return 1;
    return 0;
}
