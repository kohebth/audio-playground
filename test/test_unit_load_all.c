#include <apgcore/host/host_v2.h>
#include <apgcore/validator/unit_v2.h>

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHUNK 64u

static int has_unit_v2_yaml_suffix(const char *name) {
    size_t n = name ? strlen(name) : 0u;
    size_t s = strlen(".unit.v2.yaml");
    return n >= s && strcmp(name + n - s, ".unit.v2.yaml") == 0;
}

static int is_mono_audio_port(const apg_unit_v2_port_t *port) {
    return port && port->type && strcmp(port->type, "audio") == 0 && port->channels && strcmp(port->channels, "1") == 0;
}

static const char *single_mono_audio_port_name(const apg_unit_v2_port_t *ports, size_t ports_len) {
    const char *name = NULL;
    for (size_t i = 0; i < ports_len; i++) {
        if (!is_mono_audio_port(&ports[i]))
            continue;
        if (name)
            return NULL;
        name = ports[i].name;
    }
    return name;
}

static void fill_input(float *x, int mode) {
    for (size_t i = 0; i < TEST_CHUNK; i++) {
        if (mode == 0) {
            x[i] = 0.0f;
        } else if (mode == 1) {
            x[i] = (i == 0u) ? 1.0f : 0.0f;
        } else {
            x[i] = 0.2f * sinf(2.0f * 3.14159265358979323846f * 440.0f * (float)i / 48000.0f);
        }
    }
}

static int output_is_sane(const float *y, const char *path, int mode) {
    float peak = 0.0f;
    for (size_t i = 0; i < TEST_CHUNK; i++) {
        if (!isfinite(y[i])) {
            fprintf(stderr, "%s mode %d produced non-finite output at sample %zu\n", path, mode, i);
            return 0;
        }
        if (fabsf(y[i]) > peak)
            peak = fabsf(y[i]);
    }
    if (peak > 32.0f) {
        fprintf(stderr, "%s mode %d produced excessive peak %.6f\n", path, mode, peak);
        return 0;
    }
    return 1;
}

static int test_unit_file(const char *path, int *processed) {
    uc_arena metadata_arena;
    if (uc_arena_init(&metadata_arena, 1024u * 1024u) != 0)
        return 1;

    apg_unit_v2_t unit;
    uc_error      err    = {0};
    uc_status     status = apg_unit_v2_load_file(path, &metadata_arena, &unit, &err);
    if (status != UC_OK) {
        fprintf(stderr, "failed to load metadata for %s: %s\n", path, err.msg);
        uc_arena_free(&metadata_arena);
        return 1;
    }

    const char *input_port  = single_mono_audio_port_name(unit.input_ports, unit.input_ports_len);
    const char *output_port = single_mono_audio_port_name(unit.output_ports, unit.output_ports_len);

    apg_v2_host_unit_t *host = NULL;
    err                      = (uc_error){0};
    status                   = apg_v2_host_load_file(path, TEST_CHUNK, 48000.0f, &host, &err);
    if (status != UC_OK) {
        fprintf(stderr, "failed to load %s: %s\n", path, err.msg);
        uc_arena_free(&metadata_arena);
        return 1;
    }

    if (!input_port || !output_port) {
        apg_v2_host_destroy(host);
        uc_arena_free(&metadata_arena);
        return 0;
    }

    float input[TEST_CHUNK];
    float output[TEST_CHUNK];
    for (int mode = 0; mode < 3; mode++) {
        fill_input(input, mode);
        memset(output, 0, sizeof(output));
        if (!apg_v2_host_process_mono_ports(host, input_port, input, output_port, output, TEST_CHUNK)) {
            fprintf(stderr, "%s mode %d failed runtime process: %s\n", path, mode, apg_v2_host_last_error(host));
            apg_v2_host_destroy(host);
            uc_arena_free(&metadata_arena);
            return 1;
        }
        if (!output_is_sane(output, path, mode)) {
            apg_v2_host_destroy(host);
            uc_arena_free(&metadata_arena);
            return 1;
        }
    }

    *processed += 1;
    apg_v2_host_destroy(host);
    uc_arena_free(&metadata_arena);
    return 0;
}

int main(void) {
    DIR *dir = opendir("test/fixtures/units-v2");
    if (!dir) {
        perror("opendir test/fixtures/units-v2");
        return 1;
    }

    int checked   = 0;
    int processed = 0;
    int failed    = 0;
    for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
        if (!has_unit_v2_yaml_suffix(entry->d_name))
            continue;

        char path[512];
        snprintf(path, sizeof(path), "test/fixtures/units-v2/%s", entry->d_name);
        checked++;
        failed += test_unit_file(path, &processed);
    }
    closedir(dir);

    if (checked == 0 || processed == 0) {
        fprintf(stderr, "checked %d v2 unit fixtures and processed %d mono fixtures\n", checked, processed);
        return 1;
    }
    return failed ? 1 : 0;
}
