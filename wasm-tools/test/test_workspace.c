#include <apg/wasm/abi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char          *path;
    apg_wasm_file_role_t role;
} fixture_file_t;

static const fixture_file_t fixture_files[] = {
    {"test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml", APG_WASM_FILE_PROJECT},
    {             "test/fixtures/units-v2/noise_gate.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {                 "test/fixtures/units-v2/phaser.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {              "test/fixtures/units-v2/overdrive.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {             "test/fixtures/units-v2/tone_stack.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {                "test/fixtures/units-v2/tremolo.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {                 "test/fixtures/units-v2/chorus.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {                  "test/fixtures/units-v2/delay.unit.v2.yaml",    APG_WASM_FILE_UNIT},
    {       "test/fixtures/units-v2/schroeder_reverb.unit.v2.yaml",    APG_WASM_FILE_UNIT},
};

static const fixture_file_t extreme_atom_files[] = {
    {"test/fixtures/projects-v2/perf/extreme-atoms.project.v2.yaml", APG_WASM_FILE_PROJECT},
    {    "test/fixtures/units-v2/perf/perf_atoms_1000.unit.v2.yaml",    APG_WASM_FILE_UNIT},
};

static int fail(const apg_wasm_control_t *control, const char *message) {
    const apg_wasm_diagnostic_t *diagnostic = apg_wasm_control_last_diagnostic(control);
    fprintf(
        stderr, "FAIL: %s: %s (%s %s)\n", message, diagnostic && diagnostic->message ? diagnostic->message : "",
        diagnostic && diagnostic->phase ? diagnostic->phase : "", diagnostic && diagnostic->code ? diagnostic->code : ""
    );
    return 1;
}

static char *read_fixture(const char *relative_path, size_t *out_len) {
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", APG_TEST_ROOT, relative_path);
    FILE *file = fopen(full_path, "rb");
    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
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

static int put_fixture(apg_wasm_control_t *control, const fixture_file_t *fixture) {
    size_t content_len = 0u;
    char  *content     = read_fixture(fixture->path, &content_len);
    if (!content)
        return fail(control, "cannot read fixture");
    const apg_wasm_status_t status =
        apg_wasm_control_put_file(control, fixture->role, fixture->path, strlen(fixture->path), content, content_len);
    free(content);
    return status == APG_WASM_STATUS_OK ? 0 : fail(control, "cannot add fixture to workspace");
}

static int put_text(apg_wasm_control_t *control, apg_wasm_file_role_t role, const char *path, const char *content) {
    return apg_wasm_control_put_file(control, role, path, strlen(path), content, strlen(content)) == APG_WASM_STATUS_OK
               ? 0
               : fail(control, "cannot add text file to workspace");
}

int main(void) {
    const char         *entry   = fixture_files[0].path;
    apg_wasm_control_t *control = apg_wasm_control_create(0u);
    if (!control)
        return fail(NULL, "cannot create control module");

    if (apg_wasm_control_begin_workspace(control, 1u, entry, strlen(entry)) != APG_WASM_STATUS_OK)
        return fail(control, "cannot begin workspace");
    for (size_t i = 0u; i < sizeof(fixture_files) / sizeof(fixture_files[0]); ++i) {
        if (put_fixture(control, &fixture_files[i]))
            return 1;
    }

    if (apg_wasm_control_validate_workspace(control) != APG_WASM_STATUS_OK)
        return fail(control, "in-memory workspace validation failed");
    if (apg_wasm_control_compile_workspace(control) != APG_WASM_STATUS_OK)
        return fail(control, "in-memory workspace compilation failed");
    const apg_wasm_workspace_summary_t *summary = apg_wasm_control_workspace_summary(control);
    if (!summary || summary->revision != 1u || summary->unit_count != 8u || summary->instance_count != 8u ||
        summary->node_count == 0u || summary->schedule_count == 0u || summary->signal_count == 0u ||
        summary->param_count == 0u)
        return fail(control, "compiled workspace summary is incomplete");

    if (apg_wasm_control_begin_workspace(control, 1u, entry, strlen(entry)) != APG_WASM_STATUS_STALE_REVISION)
        return fail(control, "stale workspace revision was accepted");

    if (apg_wasm_control_begin_workspace(control, 2u, entry, strlen(entry)) != APG_WASM_STATUS_OK)
        return fail(control, "cannot begin missing-unit workspace");
    if (put_fixture(control, &fixture_files[0]))
        return 1;
    if (apg_wasm_control_validate_workspace(control) != APG_WASM_STATUS_VALIDATION_ERROR)
        return fail(control, "missing unit was not rejected");
    const apg_wasm_diagnostic_t *diagnostic = apg_wasm_control_last_diagnostic(control);
    if (!diagnostic || diagnostic->revision != 2u || !diagnostic->code ||
        strcmp(diagnostic->code, "APG_UNIT_MISSING") != 0)
        return fail(control, "missing-unit diagnostic is incomplete");

    const char *escaping_entry = "../outside.project.v2.yaml";
    if (apg_wasm_control_begin_workspace(control, 3u, escaping_entry, strlen(escaping_entry)) !=
        APG_WASM_STATUS_INVALID_ARGUMENT)
        return fail(control, "escaping entry-project path was accepted");
    if (apg_wasm_control_begin_workspace(control, 3u, entry, strlen(entry)) != APG_WASM_STATUS_OK)
        return fail(control, "valid revision was consumed by rejected path");
    const char *yaml = "kind: apg.unit\n";
    if (apg_wasm_control_put_file(
            control, APG_WASM_FILE_UNIT, "../outside.unit.v2.yaml", strlen("../outside.unit.v2.yaml"), yaml,
            strlen(yaml)
        ) != APG_WASM_STATUS_INVALID_ARGUMENT)
        return fail(control, "escaping workspace file path was accepted");

    if (apg_wasm_control_begin_workspace(control, 4u, entry, strlen(entry)) != APG_WASM_STATUS_OK)
        return fail(control, "cannot begin invalid-draft workspace");
    for (size_t i = 0u; i < sizeof(fixture_files) / sizeof(fixture_files[0]); ++i) {
        if (put_fixture(control, &fixture_files[i]))
            return 1;
    }
    const char *invalid_unit = "kind: apg.unit\n"
                               "schema: apg.unit.v2\n"
                               "name: broken_draft\n"
                               "version: 1.0.0\n"
                               "params:\n"
                               "  gain:\n"
                               "    type: float\n"
                               "    default: 1\n"
                               "    min: 0\n"
                               "    max: 4\n"
                               "ports:\n"
                               "  inputs:\n"
                               "    - name: input\n"
                               "      type: audio\n"
                               "      channels: 1\n"
                               "  outputs:\n"
                               "    - name: output\n"
                               "      type: audio\n"
                               "      channels: 1\n"
                               "graph:\n"
                               "  signals:\n"
                               "    - input\n"
                               "    - output\n"
                               "  nodes:\n"
                               "    - id: broken\n"
                               "      atom: amplitude_multiply\n"
                               "      in:\n"
                               "        signal_a: input\n"
                               "      out:\n"
                               "        signal: output\n"
                               "compatibility:\n"
                               "  desktop_full: true\n";
    if (put_text(control, APG_WASM_FILE_UNIT, "workspace/broken.unit.v2.yaml", invalid_unit))
        return 1;
    if (apg_wasm_control_validate_workspace(control) != APG_WASM_STATUS_COMPILE_ERROR)
        return fail(control, "invalid unreferenced unit draft was accepted");
    diagnostic = apg_wasm_control_last_diagnostic(control);
    if (!diagnostic || strcmp(diagnostic->file, "workspace/broken.unit.v2.yaml") != 0 ||
        strcmp(diagnostic->phase, "compile") != 0)
        return fail(control, "invalid unit draft diagnostic is incomplete");

    if (apg_wasm_control_begin_workspace(control, 5u, entry, strlen(entry)) != APG_WASM_STATUS_OK)
        return fail(control, "cannot begin valid-draft workspace");
    for (size_t i = 0u; i < sizeof(fixture_files) / sizeof(fixture_files[0]); ++i) {
        if (put_fixture(control, &fixture_files[i]))
            return 1;
    }
    const char *valid_unit = "kind: apg.unit\n"
                             "schema: apg.unit.v2\n"
                             "name: browser_gain\n"
                             "version: 1.0.0\n"
                             "params:\n"
                             "  gain:\n"
                             "    type: float\n"
                             "    default: 1\n"
                             "    min: 0\n"
                             "    max: 4\n"
                             "ports:\n"
                             "  inputs:\n"
                             "    - name: input\n"
                             "      type: audio\n"
                             "      channels: 1\n"
                             "  outputs:\n"
                             "    - name: output\n"
                             "      type: audio\n"
                             "      channels: 1\n"
                             "graph:\n"
                             "  signals:\n"
                             "    - input\n"
                             "    - gain_value\n"
                             "    - output\n"
                             "  nodes:\n"
                             "    - id: gain_value\n"
                             "      atom: generation_dc\n"
                             "      out:\n"
                             "        signal: gain_value\n"
                             "      config:\n"
                             "        value: ${params.gain}\n"
                             "    - id: apply_gain\n"
                             "      atom: amplitude_multiply\n"
                             "      in:\n"
                             "        signal_a: input\n"
                             "        signal_b: gain_value\n"
                             "      out:\n"
                             "        signal: output\n"
                             "compatibility:\n"
                             "  desktop_full: true\n"
                             "  wasm_realtime: true\n";
    if (put_text(control, APG_WASM_FILE_UNIT, "workspace/browser_gain.unit.v2.yaml", valid_unit))
        return 1;
    if (apg_wasm_control_validate_workspace(control) != APG_WASM_STATUS_OK ||
        apg_wasm_control_compile_workspace(control) != APG_WASM_STATUS_OK)
        return fail(control, "valid unreferenced unit draft did not validate and compile");

    const char *extreme_entry = extreme_atom_files[0].path;
    if (apg_wasm_control_begin_workspace(control, 6u, extreme_entry, strlen(extreme_entry)) != APG_WASM_STATUS_OK)
        return fail(control, "cannot begin 1,000-atom workspace");
    for (size_t i = 0u; i < sizeof(extreme_atom_files) / sizeof(extreme_atom_files[0]); ++i) {
        if (put_fixture(control, &extreme_atom_files[i]))
            return 1;
    }
    if (apg_wasm_control_validate_workspace(control) != APG_WASM_STATUS_OK ||
        apg_wasm_control_compile_workspace(control) != APG_WASM_STATUS_OK)
        return fail(control, "1,000-atom workspace did not validate and compile");
    summary = apg_wasm_control_workspace_summary(control);
    if (!summary || summary->node_count != 1000u || summary->schedule_count != 1000u)
        return fail(control, "1,000-atom workspace summary is incomplete");

    apg_wasm_control_destroy(control);
    return 0;
}
