extern "C" {
#include <atom/dsp_atoms.h>
}

static_assert(WAVEFORM_NOISE_BROWN == 6, "waveform enum value changed");
static_assert(NORMALIZE_RMS == 1, "normalize enum value changed");
static_assert(INTERPOLATION_LAGRANGE == 3, "interpolation enum value changed");
static_assert(WINDOW_RECTANGULAR == 3, "window enum value changed");

int main() {
    const auto          legacy          = &generation_dc;
    const auto          process         = &generation_dc_process;
    apg_waveform_type_t waveform        = WAVEFORM_SINE;
    WaveformType        legacy_waveform = waveform;
    (void)legacy;
    (void)process;
    (void)legacy_waveform;
    return 0;
}
