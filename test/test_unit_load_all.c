#include <runtime.h>

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_CHUNK 512

static int has_unit_yaml_suffix(const char *name) {
    size_t n = strlen(name);
    const char *suffix = ".unit.yaml";
    size_t s = strlen(suffix);
    return n >= s && strcmp(name + n - s, suffix) == 0;
}

static void fill_input(float *x, int mode) {
    for (int i = 0; i < TEST_CHUNK; i++) {
        if (mode == 0) x[i] = 0.0f;
        else if (mode == 1) x[i] = (i == 0) ? 1.0f : 0.0f;
        else if (mode == 2) x[i] = 0.2f * sinf(2.0f * 3.14159265358979323846f * 440.0f * (float)i / 48000.0f);
        else {
            unsigned v = (unsigned)(i * 1103515245u + 12345u);
            x[i] = ((float)(v & 0xffffu) / 32768.0f - 1.0f) * 0.05f;
        }
    }
}

static int assert_output_sane(const float *y, const char *path, int mode) {
    float peak = 0.0f;
    for (int i = 0; i < TEST_CHUNK; i++) {
        if (!isfinite(y[i])) {
            fprintf(stderr, "%s mode %d produced non-finite output at sample %d\n", path, mode, i);
            return 1;
        }
        if (fabsf(y[i]) > peak) peak = fabsf(y[i]);
    }
    if (peak > 32.0f) {
        fprintf(stderr, "%s mode %d produced excessive peak %.6f\n", path, mode, peak);
        return 1;
    }
    return 0;
}

static int test_unit_file(const char *path) {
    runtime_context_t ctx = {.sample_rate = 48000, .chunk_length = TEST_CHUNK};
    runtime_unit_t *unit = runtime_unit_load(path, ctx);
    if (!unit) {
        fprintf(stderr, "failed to load %s\n", path);
        return 1;
    }

    float in[TEST_CHUNK];
    float out[TEST_CHUNK];
    int rc = 0;
    for (int mode = 0; mode < 4; mode++) {
        fill_input(in, mode);
        memset(out, 0, sizeof(out));
        runtime_unit_process(unit, in, out);
        if (assert_output_sane(out, path, mode)) {
            rc = 1;
            break;
        }
    }

    runtime_unit_destroy(unit);
    return rc;
}

int main(void) {
    DIR *dir = opendir("units");
    if (!dir) {
        perror("opendir units");
        return 1;
    }

    int checked = 0;
    int failed = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!has_unit_yaml_suffix(entry->d_name)) continue;

        char path[512];
        snprintf(path, sizeof(path), "units/%s", entry->d_name);
        checked++;
        failed += test_unit_file(path);
    }
    closedir(dir);

    if (checked == 0) {
        fprintf(stderr, "no unit yaml files checked\n");
        return 1;
    }
    return failed ? 1 : 0;
}
