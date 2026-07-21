#include <apgcore/validator/project_v2.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static int expect_valid_fixture(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_t project;
    uc_error         err = {0};
    uc_status        status =
        apg_project_v2_load_file("test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", &arena, &project, &err);
    if (status != UC_OK) {
        fprintf(stderr, "project validation error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("simple project fixture did not validate");
    }

    if (!project.name || strcmp(project.name, "simple-gain-board") != 0)
        return fail("unexpected project name");
    if (!project.version || strcmp(project.version, "2.0.0") != 0)
        return fail("unexpected project version");
    if (project.units_len != 1u || strcmp(project.units[0].id, "gain_unit") != 0 ||
        strcmp(project.units[0].file, "../units-v2/simple_gain.unit.v2.yaml") != 0)
        return fail("unexpected project unit refs");
    if (project.nodes_len != 1u || strcmp(project.nodes[0].id, "gain1") != 0 ||
        strcmp(project.nodes[0].unit, "gain_unit") != 0 || project.nodes[0].params_len != 1u ||
        strcmp(project.nodes[0].params[0].key, "gain") != 0 ||
        strcmp(project.nodes[0].params[0].value.text, "2.0") != 0)
        return fail("unexpected project chain node");
    if (project.routes_len != 2u || strcmp(project.routes[0].from, "system.input") != 0 ||
        strcmp(project.routes[0].to, "gain1.input") != 0 || strcmp(project.routes[1].from, "gain1.output") != 0 ||
        strcmp(project.routes[1].to, "system.output") != 0)
        return fail("unexpected project routes");
    if (project.scenes_len != 2u || strcmp(project.scenes[1].name, "Boost") != 0 ||
        project.scenes[1].params_len != 1u || strcmp(project.scenes[1].params[0].key, "gain1.gain") != 0 ||
        project.scenes[0].bypass_len != 1u || project.scenes[0].bypass[0].bypassed ||
        strcmp(project.scenes[0].bypass[0].instance, "gain1") != 0 || project.scenes[1].bypass_len != 1u ||
        !project.scenes[1].bypass[0].bypassed)
        return fail("unexpected project scenes");
    if (!project.targets.default_profile || strcmp(project.targets.default_profile, "desktop_full") != 0 ||
        project.targets.export_profiles_len != 2u || strcmp(project.targets.export_profiles[0], "wasm_realtime") != 0)
        return fail("unexpected project targets");

    uc_arena_free(&arena);
    return 0;
}

static int expect_valid_resolved_fixture(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t resolved;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(
        "test/fixtures/projects-v2/simple-gain-board.project.v2.yaml", &arena, &resolved, &err
    );
    if (status != UC_OK) {
        fprintf(stderr, "resolved project error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("simple project fixture did not resolve");
    }

    if (resolved.units_len != 1u || strcmp(resolved.units[0].id, "gain_unit") != 0)
        return fail("unexpected resolved unit ref");
    if (!resolved.units[0].resolved_path ||
        !strstr(resolved.units[0].resolved_path, "/test/fixtures/units-v2/simple_gain.unit.v2.yaml"))
        return fail("unexpected resolved unit path");
    if (!resolved.units[0].unit.name || strcmp(resolved.units[0].unit.name, "simple_gain") != 0)
        return fail("resolved unit was not loaded");

    uc_arena_free(&arena);
    return 0;
}

static int expect_valid_empty_fixture(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t resolved;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(
        "test/fixtures/projects-v2/empty-passthrough.project.v2.yaml", &arena, &resolved, &err
    );
    if (status != UC_OK) {
        fprintf(stderr, "empty project error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("empty pass-through fixture did not resolve");
    }

    if (resolved.units_len != 0u || resolved.project.units_len != 0u || resolved.project.nodes_len != 0u ||
        resolved.project.routes_len != 1u || strcmp(resolved.project.routes[0].from, "system.input") != 0 ||
        strcmp(resolved.project.routes[0].to, "system.output") != 0) {
        uc_arena_free(&arena);
        return fail("unexpected empty project shape");
    }

    uc_arena_free(&arena);
    return 0;
}

static int expect_valid_pedalboard_fixture(void) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t resolved;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(
        "test/fixtures/projects-v2/guitar-pedalboard.project.v2.yaml", &arena, &resolved, &err
    );
    if (status != UC_OK) {
        fprintf(stderr, "pedalboard project error: %s\n", err.msg);
        uc_arena_free(&arena);
        return fail("guitar pedalboard fixture did not resolve");
    }

    if (resolved.units_len != 8u || resolved.project.nodes_len != 8u || resolved.project.routes_len != 9u)
        return fail("unexpected pedalboard project shape");
    if (strcmp(resolved.units[0].unit.name, "noise_gate") != 0 || strcmp(resolved.units[1].unit.name, "phaser") != 0 ||
        strcmp(resolved.units[5].unit.name, "chorus") != 0 ||
        strcmp(resolved.units[7].unit.name, "schroeder_reverb") != 0)
        return fail("unexpected pedalboard unit order");

    uc_arena_free(&arena);
    return 0;
}

static int expect_valid_routing_fixture(const char *path, const char *label, size_t expected_nodes) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");
    apg_project_v2_resolved_t resolved;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(path, &arena, &resolved, &err);
    if (status != UC_OK) {
        fprintf(stderr, "%s routing project error: %s\n", label, err.msg);
        uc_arena_free(&arena);
        return 1;
    }
    bool found_routing_unit = false;
    bool found_section      = false;
    for (size_t i = 0; i < resolved.units_len; i++)
        found_routing_unit = found_routing_unit || resolved.units[i].unit.routing.role != NULL;
    for (size_t i = 0; i < resolved.project.nodes_len; i++)
        found_section = found_section || resolved.project.nodes[i].routing_section != NULL;
    if (resolved.project.nodes_len != expected_nodes || !found_section || !found_routing_unit) {
        uc_arena_free(&arena);
        return fail("routing metadata was not materialized");
    }
    uc_arena_free(&arena);
    return 0;
}

static int expect_invalid_resolved_file_contains(const char *path, const char *label, const char *must_contain) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_resolved_t resolved;
    uc_error                  err    = {0};
    uc_status                 status = apg_project_v2_load_resolved_file(path, &arena, &resolved, &err);
    uc_arena_free(&arena);

    if (status == UC_OK) {
        fprintf(stderr, "accepted invalid resolved project case: %s\n", label);
        return 1;
    }
    if (must_contain && !strstr(err.msg, must_contain)) {
        fprintf(stderr, "resolved project error for %s lacked '%s': %s\n", label, must_contain, err.msg);
        return 1;
    }
    return 0;
}

static int expect_invalid_contains(const char *yaml, const char *label, const char *must_contain) {
    uc_arena arena;
    if (uc_arena_init(&arena, 1024 * 1024) != 0)
        return fail("arena init failed");

    apg_project_v2_t project;
    uc_error         err    = {0};
    uc_status        status = apg_project_v2_load_string(yaml, strlen(yaml), &arena, &project, &err);
    uc_arena_free(&arena);

    if (status == UC_OK) {
        fprintf(stderr, "accepted invalid project case: %s\n", label);
        return 1;
    }
    if (must_contain && !strstr(err.msg, must_contain)) {
        fprintf(stderr, "project validation error for %s lacked '%s': %s\n", label, must_contain, err.msg);
        return 1;
    }
    return 0;
}

int main(void) {
    if (expect_valid_fixture())
        return 1;
    if (expect_valid_resolved_fixture())
        return 1;
    if (expect_valid_empty_fixture())
        return 1;
    if (expect_valid_pedalboard_fixture())
        return 1;
    if (expect_valid_routing_fixture("test/fixtures/projects-v2/parallel-gain.project.v2.yaml", "parallel", 3u))
        return 1;
    if (expect_valid_routing_fixture("test/fixtures/projects-v2/nested-parallel.project.v2.yaml", "nested", 4u))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-raw-fanout.project.v2.yaml", "raw fan-out", "Add in parallel"
        ))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-crossed-routing.project.v2.yaml", "crossed routing", "wrong mixer"
        ))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-routing-scene-bypass.project.v2.yaml", "routing scene bypass",
            "always-active"
        ))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-missing-unit.project.v2.yaml", "missing unit file",
            "cannot resolve unit file"
        ))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-absolute-unit.project.v2.yaml", "absolute unit file",
            "absolute unit file paths"
        ))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-escaping-unit.project.v2.yaml", "escaping unit file",
            "escapes workspace root"
        ))
        return 1;
    if (expect_invalid_resolved_file_contains(
            "test/fixtures/projects-v2/invalid-duplicate-resolved-unit.project.v2.yaml", "duplicate resolved unit file",
            "duplicate resolved unit file"
        ))
        return 1;

    const char *duplicate_unit = "kind: apg.project\n"
                                 "schema: apg.project.v2\n"
                                 "name: bad\n"
                                 "version: 2.0.0\n"
                                 "units:\n"
                                 "  - id: gain_unit\n"
                                 "    file: gain.yaml\n"
                                 "  - id: gain_unit\n"
                                 "    file: gain2.yaml\n"
                                 "chain:\n"
                                 "  nodes:\n"
                                 "    - id: gain1\n"
                                 "      unit: gain_unit\n"
                                 "  routes:\n"
                                 "    - from: system.input\n"
                                 "      to: gain1.input\n"
                                 "targets:\n"
                                 "  default: desktop_full\n";
    if (expect_invalid_contains(duplicate_unit, "duplicate unit", "duplicate unit"))
        return 1;

    const char *unknown_node_unit = "kind: apg.project\n"
                                    "schema: apg.project.v2\n"
                                    "name: bad\n"
                                    "version: 2.0.0\n"
                                    "units:\n"
                                    "  - id: gain_unit\n"
                                    "    file: gain.yaml\n"
                                    "chain:\n"
                                    "  nodes:\n"
                                    "    - id: gain1\n"
                                    "      unit: missing_unit\n"
                                    "  routes:\n"
                                    "    - from: system.input\n"
                                    "      to: gain1.input\n"
                                    "targets:\n"
                                    "  default: desktop_full\n";
    if (expect_invalid_contains(unknown_node_unit, "unknown node unit", "missing_unit"))
        return 1;

    const char *duplicate_node = "kind: apg.project\n"
                                 "schema: apg.project.v2\n"
                                 "name: bad\n"
                                 "version: 2.0.0\n"
                                 "units:\n"
                                 "  - id: gain_unit\n"
                                 "    file: gain.yaml\n"
                                 "chain:\n"
                                 "  nodes:\n"
                                 "    - id: gain1\n"
                                 "      unit: gain_unit\n"
                                 "    - id: gain1\n"
                                 "      unit: gain_unit\n"
                                 "  routes:\n"
                                 "    - from: system.input\n"
                                 "      to: gain1.input\n"
                                 "targets:\n"
                                 "  default: desktop_full\n";
    if (expect_invalid_contains(duplicate_node, "duplicate node", "duplicate chain node"))
        return 1;

    const char *bad_route = "kind: apg.project\n"
                            "schema: apg.project.v2\n"
                            "name: bad\n"
                            "version: 2.0.0\n"
                            "units:\n"
                            "  - id: gain_unit\n"
                            "    file: gain.yaml\n"
                            "chain:\n"
                            "  nodes:\n"
                            "    - id: gain1\n"
                            "      unit: gain_unit\n"
                            "  routes:\n"
                            "    - from: missing.output\n"
                            "      to: gain1.input\n"
                            "targets:\n"
                            "  default: desktop_full\n";
    if (expect_invalid_contains(bad_route, "bad route", "chain.routes[].from"))
        return 1;

    const char *bad_scene_param = "kind: apg.project\n"
                                  "schema: apg.project.v2\n"
                                  "name: bad\n"
                                  "version: 2.0.0\n"
                                  "units:\n"
                                  "  - id: gain_unit\n"
                                  "    file: gain.yaml\n"
                                  "chain:\n"
                                  "  nodes:\n"
                                  "    - id: gain1\n"
                                  "      unit: gain_unit\n"
                                  "  routes:\n"
                                  "    - from: system.input\n"
                                  "      to: gain1.input\n"
                                  "scenes:\n"
                                  "  - name: Bad\n"
                                  "    params:\n"
                                  "      missing.gain: 2.0\n"
                                  "targets:\n"
                                  "  default: desktop_full\n";
    if (expect_invalid_contains(bad_scene_param, "bad scene param", "missing.gain"))
        return 1;

    const char *bad_scene_bypass_instance = "kind: apg.project\n"
                                            "schema: apg.project.v2\n"
                                            "name: bad\n"
                                            "version: 2.0.0\n"
                                            "units:\n"
                                            "  - id: gain_unit\n"
                                            "    file: gain.yaml\n"
                                            "chain:\n"
                                            "  nodes:\n"
                                            "    - id: gain1\n"
                                            "      unit: gain_unit\n"
                                            "  routes:\n"
                                            "    - from: system.input\n"
                                            "      to: gain1.input\n"
                                            "scenes:\n"
                                            "  - name: Bad\n"
                                            "    bypass:\n"
                                            "      missing: true\n"
                                            "targets:\n"
                                            "  default: desktop_full\n";
    if (expect_invalid_contains(bad_scene_bypass_instance, "bad scene bypass instance", "unknown instance"))
        return 1;

    const char *bad_scene_bypass_value = "kind: apg.project\n"
                                         "schema: apg.project.v2\n"
                                         "name: bad\n"
                                         "version: 2.0.0\n"
                                         "units:\n"
                                         "  - id: gain_unit\n"
                                         "    file: gain.yaml\n"
                                         "chain:\n"
                                         "  nodes:\n"
                                         "    - id: gain1\n"
                                         "      unit: gain_unit\n"
                                         "  routes:\n"
                                         "    - from: system.input\n"
                                         "      to: gain1.input\n"
                                         "scenes:\n"
                                         "  - name: Bad\n"
                                         "    bypass:\n"
                                         "      gain1: maybe\n"
                                         "targets:\n"
                                         "  default: desktop_full\n";
    if (expect_invalid_contains(bad_scene_bypass_value, "bad scene bypass value", "must be true or false"))
        return 1;

    const char *bad_target = "kind: apg.project\n"
                             "schema: apg.project.v2\n"
                             "name: bad\n"
                             "version: 2.0.0\n"
                             "units:\n"
                             "  - id: gain_unit\n"
                             "    file: gain.yaml\n"
                             "chain:\n"
                             "  nodes:\n"
                             "    - id: gain1\n"
                             "      unit: gain_unit\n"
                             "  routes:\n"
                             "    - from: system.input\n"
                             "      to: gain1.input\n"
                             "targets:\n"
                             "  default: browser_full\n";
    if (expect_invalid_contains(bad_target, "bad target", "browser_full"))
        return 1;

    const char *duplicate_export_target = "kind: apg.project\n"
                                          "schema: apg.project.v2\n"
                                          "name: bad\n"
                                          "version: 2.0.0\n"
                                          "units:\n"
                                          "  - id: gain_unit\n"
                                          "    file: gain.yaml\n"
                                          "chain:\n"
                                          "  nodes:\n"
                                          "    - id: gain1\n"
                                          "      unit: gain_unit\n"
                                          "  routes:\n"
                                          "    - from: system.input\n"
                                          "      to: gain1.input\n"
                                          "targets:\n"
                                          "  default: desktop_full\n"
                                          "  export:\n"
                                          "    - wasm_realtime\n"
                                          "    - wasm_realtime\n";
    if (expect_invalid_contains(duplicate_export_target, "duplicate export target", "duplicate targets.export"))
        return 1;

    return 0;
}
