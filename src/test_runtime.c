#include <runtime.h>
#include <stdio.h>
#include <math.h>

int main() {
    runtime_context_t rt_ctx = {.sample_rate = 48000, .chunk_length = 512};
    runtime_unit_t *unit = runtime_unit_load("../units/electric_piano.unit.yaml", rt_ctx);
    if (!unit) {
        printf("Failed to load unit\n");
        return 1;
    }
    
    float in[512] = {0};
    float out[512];
    
    // Find the latched_freq signal to monitor it
    float *latched_freq_sig = NULL;
    for (int i = 0; i < unit->n_signals; i++) {
        if (strcmp(unit->signals[i].name, "latched_freq") == 0) {
            latched_freq_sig = unit->signals[i].buffer;
            break;
        }
    }
    
    FILE *fp = fopen("../units/test_audio.raw", "rb");
    if (!fp) {
        printf("Failed to open test_audio.raw\n");
        return 1;
    }

    int chunk = 0;
    while (fread(in, sizeof(float), 512, fp) == 512) {
        runtime_unit_process(unit, in, out);
        
        float rms = 0;
        for(int i=0; i<512; i++) rms += out[i] * out[i];
        rms = sqrtf(rms / 512.0f);
        
        float track_f = 0.0f, quant_f = 0.0f, latch_f = 0.0f;
        for (int i = 0; i < unit->n_signals; i++) {
            if (strcmp(unit->signals[i].name, "tracked_freq") == 0) track_f = unit->signals[i].buffer[0];
            if (strcmp(unit->signals[i].name, "quantized_freq") == 0) quant_f = unit->signals[i].buffer[0];
            if (strcmp(unit->signals[i].name, "latched_freq") == 0) latch_f = unit->signals[i].buffer[0];
        }
        
        if (latch_f > 0.0f || track_f > 0.0f) {
            printf("Chunk %3d: RMS=%.6f, Tracked=%.2f, Quantized=%.2f, Latched=%.2f\n", chunk, rms, track_f, quant_f, latch_f);
        }
        chunk++;
    }
    
    fclose(fp);
    return 0;
}
