#include <atom/dsp_atoms.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>

#define CHUNK_LENGTH 512

// Lookup table for the frequencies of all 128 MIDI notes
static float midi_freqs[128];
static int table_initialized = 0;

static void init_midi_table() {
    for (int i = 0; i < 128; i++) {
        midi_freqs[i] = 440.0f * powf(2.0f, (float)(i - 69) / 12.0f);
    }
    table_initialized = 1;
}

void freq_quantize(
    freq_quantize_out_t    *out,
    freq_quantize_in_t     *in,
    freq_quantize_params_t *params,
    freq_quantize_state_t  *state
) {
    if (out->signal == NULL || in->signal == NULL)
        return;

    if (!table_initialized) {
        init_midi_table();
    }

    for (int i = 0; i < CHUNK_LENGTH; ++i) {
        float freq = in->signal[i];
        
        // Skip silence or extremely low frequencies
        if (freq < 20.0f) {
            out->signal[i] = 0.0f;
            continue;
        }

        // Fast Binary Search to find the nearest frequency in the LUT
        // This avoids expensive log2f() and powf() calls per sample
        int low = 0;
        int high = 127;
        
        while (low <= high) {
            int mid = (low + high) / 2;
            if (freq < midi_freqs[mid]) {
                high = mid - 1;
            } else if (freq > midi_freqs[mid]) {
                low = mid + 1;
            } else {
                low = mid;
                high = mid;
                break;
            }
        }

        if (high < 0) {
            out->signal[i] = midi_freqs[0];
        } else if (low > 127) {
            out->signal[i] = midi_freqs[127];
        } else {
            // high is the lower bound note, low is the upper bound note
            float diff_down = freq - midi_freqs[high];
            float diff_up   = midi_freqs[low] - freq;
            out->signal[i]  = (diff_down < diff_up) ? midi_freqs[high] : midi_freqs[low];
        }
    }
}
