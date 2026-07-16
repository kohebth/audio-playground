#include <atom/types/dsp_enums.h>

_Static_assert(WAVEFORM_SINE == 0, "WAVEFORM_SINE value changed");
_Static_assert(WAVEFORM_SAW == 1, "WAVEFORM_SAW value changed");
_Static_assert(WAVEFORM_SQUARE == 2, "WAVEFORM_SQUARE value changed");
_Static_assert(WAVEFORM_TRIANGLE == 3, "WAVEFORM_TRIANGLE value changed");
_Static_assert(WAVEFORM_NOISE_WHITE == 4, "WAVEFORM_NOISE_WHITE value changed");
_Static_assert(WAVEFORM_NOISE_PINK == 5, "WAVEFORM_NOISE_PINK value changed");
_Static_assert(WAVEFORM_NOISE_BROWN == 6, "WAVEFORM_NOISE_BROWN value changed");
_Static_assert(NORMALIZE_PEAK == 0, "NORMALIZE_PEAK value changed");
_Static_assert(NORMALIZE_RMS == 1, "NORMALIZE_RMS value changed");
_Static_assert(INTERPOLATION_LINEAR == 0, "INTERPOLATION_LINEAR value changed");
_Static_assert(INTERPOLATION_CUBIC == 1, "INTERPOLATION_CUBIC value changed");
_Static_assert(INTERPOLATION_SINC == 2, "INTERPOLATION_SINC value changed");
_Static_assert(INTERPOLATION_LAGRANGE == 3, "INTERPOLATION_LAGRANGE value changed");
_Static_assert(WINDOW_HANN == 0, "WINDOW_HANN value changed");
_Static_assert(WINDOW_HAMMING == 1, "WINDOW_HAMMING value changed");
_Static_assert(WINDOW_BLACKMAN == 2, "WINDOW_BLACKMAN value changed");
_Static_assert(WINDOW_RECTANGULAR == 3, "WINDOW_RECTANGULAR value changed");

void apg_header_smoke_dsp_enums(void) {
    enum apg_waveform_type      waveform_tag         = WAVEFORM_SINE;
    apg_waveform_type_t         waveform             = waveform_tag;
    WaveformType                legacy_waveform      = waveform;
    enum apg_normalize_mode     normalize_tag        = NORMALIZE_PEAK;
    apg_normalize_mode_t        normalize            = normalize_tag;
    NormalizeMode               legacy_normalize     = normalize;
    enum apg_interpolation_type interpolation_tag    = INTERPOLATION_LINEAR;
    apg_interpolation_type_t    interpolation        = interpolation_tag;
    InterpolationType           legacy_interpolation = interpolation;
    enum apg_window_type        window_tag           = WINDOW_HANN;
    apg_window_type_t           window               = window_tag;
    WindowType                  legacy_window        = window;

    (void)legacy_waveform;
    (void)legacy_normalize;
    (void)legacy_interpolation;
    (void)legacy_window;
}
