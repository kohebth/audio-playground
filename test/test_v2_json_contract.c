#include <apgcore/host/json_contract_v2.h>
#include <apgcore/metadata/atom_catalog.h>

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
    size_t expected_len = strlen(expected);
    while (expected_len > 0u && (expected[expected_len - 1u] == '\n' || expected[expected_len - 1u] == '\r'))
        expected[--expected_len] = '\0';
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

static int test_empty_project_and_scene_contracts(void) {
    const char *empty_path = "test/fixtures/projects-v2/empty-passthrough.project.v2.yaml";
    char       *validation = capture_json(apg_v2_json_write_validate_project, empty_path);
    char       *inspect    = capture_json(apg_v2_json_write_inspect_project, empty_path);
    char       *render     = capture_json(apg_v2_json_write_render_project, empty_path);
    char       *scenes =
        capture_json(apg_v2_json_write_inspect_project, "test/fixtures/projects-v2/simple-gain-board.project.v2.yaml");
    if (!validation || !inspect || !render || !scenes) {
        free(validation);
        free(inspect);
        free(render);
        free(scenes);
        return fail("failed to write empty project or scene json");
    }

    int ok = strstr(validation, "\"schema\":\"apg.validation.v2\"") && strstr(validation, "\"ok\":true") &&
             strstr(inspect, "\"units\":[]") && strstr(inspect, "\"nodes\":[]") &&
             strstr(inspect, "\"routes\":[{\"from\":\"system.input\",\"to\":\"system.output\"}]") &&
             strstr(inspect, "\"scenes\":[]") &&
             strstr(inspect, "\"compiled\":{\"params\":0,\"signals\":1,\"nodes\":0,\"schedule\":0}") &&
             strstr(render, "\"schema\":\"apg.project.render.v2\"") && strstr(render, "\"ok\":true") &&
             strstr(scenes, "\"bypass\":{\"gain1\":false}") && strstr(scenes, "\"bypass\":{\"gain1\":true}");

    free(validation);
    free(inspect);
    free(render);
    free(scenes);
    return ok ? 0 : fail("empty project or scene json contract was incomplete");
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

static int test_routing_inspect_json_contract(void) {
    char *unit = capture_json(apg_v2_json_write_inspect_unit, "test/fixtures/units-v2/path_panner_2.unit.v2.yaml");
    char *project =
        capture_json(apg_v2_json_write_inspect_project, "test/fixtures/projects-v2/parallel-gain.project.v2.yaml");
    if (!unit || !project) {
        free(unit);
        free(project);
        return fail("failed to write routing inspect json");
    }
    int ok = strstr(
                 unit, "\"routing\":{\"role\":\"panner\",\"paths\":[{\"port\":\"path_1\",\"level_param\":"
                       "\"path_1_db\"},{\"port\":\"path_2\",\"level_param\":\"path_2_db\"}]}"
             ) &&
             strstr(unit, "\"control\":\"knob\",\"unit\":\"dB\"") &&
             strstr(
                 project, "\"id\":\"parallel_pan\",\"unit\":\"path_panner_2_unit\",\"routing\":{\"section\":"
                          "\"parallel_1\"}"
             ) &&
             strstr(project, "\"name\":\"path_mixer_2\",\"routing\":{\"role\":\"mixer\"");
    free(unit);
    free(project);
    return ok ? 0 : fail("routing inspect json lacked helper, path, section, or knob metadata");
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
    char *json     = capture_atom_catalog();
    char *expected = read_file("test/golden/v2-inspect-atoms.json");
    if (!json || !expected) {
        free(json);
        free(expected);
        return fail("failed to write atom catalog json");
    }
    if (!strstr(json, "\"schema\":\"apg.atom_catalog.v2\"") || !strstr(json, "\"name\":\"generation_dc\"") ||
        !strstr(json, "\"name\":\"coefficients\",\"type\":\"float_matrix\"")) {
        free(json);
        free(expected);
        return fail("atom inspect json lacked expected catalog fields");
    }
    int matches_golden = strcmp(json, expected) == 0;
    if (!matches_golden) {
        size_t actual_len   = strlen(json);
        size_t expected_len = strlen(expected);
        size_t first_diff   = 0u;
        while (first_diff < actual_len && first_diff < expected_len && json[first_diff] == expected[first_diff])
            first_diff++;
        fprintf(
            stderr, "atom catalog mismatch at byte %zu (actual length %zu, expected length %zu)\n", first_diff,
            actual_len, expected_len
        );
    }
    free(json);
    free(expected);
    return matches_golden ? 0 : fail("atom inspect json changed from the frozen sample contract");
}

int main(void) {
    if (test_validate_json_golden_outputs())
        return 1;
    if (test_project_inspect_json_golden_output())
        return 1;
    if (test_empty_project_and_scene_contracts())
        return 1;
    if (test_project_render_json_is_deterministic())
        return 1;
    if (test_pedalboard_fixture_golden_outputs())
        return 1;
    if (test_unit_inspect_json_golden_output())
        return 1;
    if (test_unit_inspect_json_contains_ui_contract())
        return 1;
    if (test_routing_inspect_json_contract())
        return 1;
    if (test_invalid_validation_json_contains_diagnostic_fields())
        return 1;
    if (test_atom_inspect_json_is_available())
        return 1;
    return 0;
}
