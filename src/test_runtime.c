#include <runtime.h>
#include <stdio.h>
#include <math.h>

int main() {
    runtime_context_t rt_ctx = {.sample_rate = 48000, .chunk_length = 512};
    runtime_unit_t *unit = runtime_unit_load("../units/overdrive.unit.yaml", rt_ctx);
    if (!unit) {
        printf("Failed to load unit\n");
        return 1;
    }
    
    float in[512];
    float out[512];
    for (int i = 0; i < 512; i++) in[i] = (float)sin(2.0 * M_PI * 440.0 * i / 48000.0); 
    
    for (int chunk = 0; chunk < 50; chunk++) {
        runtime_unit_process(unit, in, out);
        
        float rms = 0;
        int nan_count = 0;
        for(int i=0; i<512; i++) {
            if (isnan(out[i])) nan_count++;
            else rms += out[i] * out[i];
        }
        rms = sqrtf(rms / 512.0f);
        printf("Chunk %d: RMS=%.6f, NaNs=%d, out[0]=%.6f\n", chunk, rms, nan_count, out[0]);
    }
    
    return 0;
}
