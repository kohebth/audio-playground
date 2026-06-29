#include <apgcore/atom_catalog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static char *read_catalog_json(void) {
    FILE *file = tmpfile();
    if (!file)
        return NULL;
    apg_atom_catalog_write_json(file);
    long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

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

int main(void) {
    char *json = read_catalog_json();
    if (!json)
        return fail("failed to write atom catalog json");

    if (!strstr(json, "\"schema\":\"apg.atom_catalog.v1\""))
        return fail("catalog schema is missing");
    if (!strstr(json, "\"name\":\"generation_dc\""))
        return fail("generation_dc atom is missing");
    if (!strstr(json, "\"category\":\"generation\""))
        return fail("generation category is missing");
    if (!strstr(json, "\"outputs\":[{\"name\":\"signal\",\"type\":\"signal\"}]"))
        return fail("generation_dc output contract is missing");
    if (!strstr(json, "\"config\":[{\"name\":\"value\",\"type\":\"scalar\"}]"))
        return fail("generation_dc config contract is missing");
    if (!strstr(json, "\"name\":\"mix_matrix\""))
        return fail("mix_matrix atom is missing");
    if (!strstr(json, "\"name\":\"signals\",\"type\":\"signal_array\""))
        return fail("signal array contract is missing");
    if (!strstr(json, "\"name\":\"coefficients\",\"type\":\"float_matrix\""))
        return fail("float matrix contract is missing");
    if (!strstr(json, "\"name\":\"delay_line\""))
        return fail("delay_line atom is missing");
    if (!strstr(json, "\"stateful\":true"))
        return fail("stateful flag is missing");
    if (!strstr(json, "\"buffer_samples\":192000"))
        return fail("state buffer capacity is missing");
    if (!strstr(json, "\"profiles\":{\"desktop_full\":true"))
        return fail("profile hints are missing");
    if (!strstr(json, "\"m7_static\":false"))
        return fail("restricted profile hint is missing");

    free(json);
    return 0;
}
