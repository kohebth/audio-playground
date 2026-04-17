#include <atom/dsp_atoms.h>
#include <string.h>

#define CHUNK_LENGTH 512
#define MAX_CHANNELS 8

Buffer *mix_matrix(Buffer *signals, MixMatrixParams params) {
    if (signals == NULL || params.coefficients == NULL) return NULL;
    
    static float output_data[MAX_CHANNELS][CHUNK_LENGTH];
    static Buffer output_pointers[MAX_CHANNELS];
    static int initialized = 0;
    
    if (!initialized) {
        for (int i = 0; i < MAX_CHANNELS; i++) {
            output_pointers[i] = output_data[i];
        }
        initialized = 1;
    }
    
    uint32_t num_in = params.num_inputs;
    uint32_t num_out = params.num_outputs;
    if (num_out > MAX_CHANNELS) num_out = MAX_CHANNELS;
    
    // Reset output buffers
    for (uint32_t j = 0; j < num_out; j++) {
        memset(output_pointers[j], 0, CHUNK_LENGTH * sizeof(float));
    }
    
    // Matrix Multiplication
    for (uint32_t j = 0; j < num_out; j++) {
        for (uint32_t i = 0; i < num_in; i++) {
            float g = params.coefficients[j][i];
            for (int k = 0; k < CHUNK_LENGTH; k++) {
                output_pointers[j][k] += signals[i][k] * g;
            }
        }
    }
    
    return output_pointers;
}
