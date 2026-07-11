#include "test_atom_basic_common.h"
#include <limits.h>

static int test_detector_safety_and_pitch_accuracy(void) {
    float              input[1024];
    float              output[1025];
    static float       rms_buffer[APG_DETECT_RMS_CAPACITY];
    static float       correlation_buffer[APG_DETECT_AUTOCORRELATION_CAPACITY];
    static float       pitch_buffer[APG_DETECT_AUTOCORRELATION_CAPACITY];
    apg_process_info_t info = {.sample_rate = 48000.0f, .frames = 1024u, .output_frames = 1024u, .channels = 1u};

    for (size_t i = 0; i < 1024u; ++i) {
        input[i]  = i == 7u ? NAN : 1.0f;
        output[i] = -99.0f;
    }
    output[1024] = -99.0f;
    for (size_t i = 0; i < APG_DETECT_RMS_CAPACITY; ++i)
        rms_buffer[i] = NAN;
    detect_rms_out_t    rms_out    = {.level = output};
    detect_rms_in_t     rms_in     = {.signal = input};
    detect_rms_params_t rms_params = {.window_size = INT_MAX};
    detect_rms_state_t  rms_state  = {.buffer = rms_buffer, .write_pos = -1, .sum = NAN};
    detect_rms_process(&rms_out, &rms_in, &rms_params, &rms_state, &info);
    if (assert_finite_buffer(output, 1024, "detect_rms_process invalid state"))
        return 1;
    if (!isfinite(rms_state.sum) || rms_state.sum < 0.0f || rms_state.write_pos < 0 ||
        rms_state.write_pos >= (int)APG_DETECT_RMS_CAPACITY || output[1024] != -99.0f)
        return fail("detect_rms_process did not normalize state or preserve sentinel");

    memset(rms_buffer, 0, sizeof(rms_buffer));
    rms_params.window_size = 0;
    rms_state.write_pos    = 10;
    rms_state.sum          = -1.0f;
    info.frames            = 4u;
    detect_rms_process(&rms_out, &rms_in, &rms_params, &rms_state, &info);
    for (size_t i = 0; i < 4u; ++i) {
        const float expected = i == 7u ? 0.0f : 1.0f;
        if (fabsf(output[i] - expected) > 1.0e-7f)
            return fail("detect_rms_process did not clamp single-sample window");
    }

    for (size_t i = 0; i < 256u; ++i) {
        input[i]  = i == 5u ? NAN : sinf(2.0f * (float)M_PI * (float)i / 32.0f);
        output[i] = -99.0f;
    }
    output[256] = -99.0f;
    for (size_t i = 0; i < APG_DETECT_AUTOCORRELATION_CAPACITY; ++i)
        correlation_buffer[i] = NAN;
    detect_autocorrelate_out_t    correlation_out    = {.correlation = output};
    detect_autocorrelate_in_t     correlation_in     = {.signal = input};
    detect_autocorrelate_params_t correlation_params = {.max_lag = INT_MAX};
    detect_autocorrelate_state_t  correlation_state  = {.buffer = correlation_buffer, .write_pos = -2049};
    info.frames                                      = 256u;
    info.output_frames                               = 256u;
    detect_autocorrelate_process(&correlation_out, &correlation_in, &correlation_params, &correlation_state, &info);
    if (assert_finite_buffer(output, 256, "detect_autocorrelate_process invalid state"))
        return 1;
    if (correlation_state.write_pos < 0 || correlation_state.write_pos >= (int)APG_DETECT_AUTOCORRELATION_CAPACITY ||
        output[256] != -99.0f)
        return fail("detect_autocorrelate_process did not normalize state or preserve sentinel");

    correlation_params.max_lag = 0;
    detect_autocorrelate_process(&correlation_out, &correlation_in, &correlation_params, &correlation_state, &info);
    for (size_t i = 0; i < 256u; ++i) {
        if (output[i] != 0.0f)
            return fail("detect_autocorrelate_process did not clear zero-lag output");
    }

    for (size_t i = 0; i < 1024u; ++i) {
        input[i]        = sinf(2.0f * (float)M_PI * 1000.0f * (float)i / 48000.0f);
        output[i]       = -99.0f;
        pitch_buffer[i] = 0.0f;
    }
    output[1024]                       = -99.0f;
    detect_pitch_out_t    pitch_out    = {.pitch = output};
    detect_pitch_in_t     pitch_in     = {.signal = input};
    detect_pitch_params_t pitch_params = {.max_lag = 256, .sample_rate = 8000.0f};
    detect_pitch_state_t  pitch_state  = {.buffer = pitch_buffer, .write_pos = -1};
    info.frames                        = 1024u;
    info.output_frames                 = 1024u;
    detect_pitch_process(&pitch_out, &pitch_in, &pitch_params, &pitch_state, &info);
    if (fabsf(output[0] - 1000.0f) > 25.0f)
        return fail("detect_pitch_process did not use runtime sample rate for known tone");
    if (pitch_state.write_pos < 0 || pitch_state.write_pos >= (int)APG_DETECT_AUTOCORRELATION_CAPACITY ||
        output[1024] != -99.0f)
        return fail("detect_pitch_process did not normalize state or preserve sentinel");

    for (size_t i = 0; i < 64u; ++i)
        input[i] = NAN;
    for (size_t i = 0; i < APG_DETECT_AUTOCORRELATION_CAPACITY; ++i)
        pitch_buffer[i] = NAN;
    pitch_params.max_lag  = 128;
    pitch_state.write_pos = -5;
    info.frames           = 64u;
    info.output_frames    = 64u;
    detect_pitch_process(&pitch_out, &pitch_in, &pitch_params, &pitch_state, &info);
    if (!isfinite(output[0]) || output[0] != 0.0f)
        return fail("detect_pitch_process did not reject invalid input and history");

    uint32_t noise = 1u;
    memset(pitch_buffer, 0, sizeof(pitch_buffer));
    for (size_t i = 0; i < 1024u; ++i) {
        noise    = noise * 1664525u + 1013904223u;
        input[i] = (float)(int32_t)noise / 2147483648.0f;
    }
    pitch_state.write_pos = 0;
    info.frames           = 1024u;
    info.output_frames    = 1024u;
    detect_pitch_process(&pitch_out, &pitch_in, &pitch_params, &pitch_state, &info);
    if (output[0] != 0.0f)
        return fail("detect_pitch_process accepted uncorrelated noise");

    pitch_params.max_lag = 0;
    detect_pitch_process(&pitch_out, &pitch_in, &pitch_params, &pitch_state, &info);
    if (output[0] != 0.0f)
        return fail("detect_pitch_process accepted zero lag range");

    return 0;
}

int test_process_info_remaining_detector_frame_limits(void) {
    const int frame_sizes[] = {64, 128, 256, 512, 1024};

    for (size_t c = 0; c < sizeof(frame_sizes) / sizeof(frame_sizes[0]); c++) {
        const int frames = frame_sizes[c];
        float     x[1024];
        float     y[1024];
        float     buffer[APG_DETECT_RMS_CAPACITY];
        float     autocorr_buffer[APG_DETECT_AUTOCORRELATION_CAPACITY];
        float     pitch_buffer[APG_DETECT_AUTOCORRELATION_CAPACITY];

        for (int i = 0; i < 1024; i++) {
            x[i] = (float)(i + 1);
            y[i] = -99.0f;
        }
        memset(buffer, 0, sizeof(buffer));
        memset(autocorr_buffer, 0, sizeof(autocorr_buffer));
        memset(pitch_buffer, 0, sizeof(pitch_buffer));

        apg_process_info_t info = {.sample_rate = 48000.0f, .frames = (uint32_t)frames, .channels = 1};

        detect_slope_out_t    slope_out = {.slope = y};
        detect_slope_in_t     slope_in  = {.signal = x};
        detect_slope_params_t slope_params;
        detect_slope_state_t  slope_state = {.prev_sample = 0.0f};
        detect_slope_process(&slope_out, &slope_in, &slope_params, &slope_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f)
                return fail("detect_slope_process mismatch");
        }
        if (fabsf(slope_state.prev_sample - x[frames - 1]) > 1e-7f)
            return fail("detect_slope_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_slope_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = (i % 2 == 0) ? 1.0f : -1.0f;
            y[i] = -99.0f;
        }
        detect_zero_crossing_out_t    zero_out = {.trigger = y};
        detect_zero_crossing_in_t     zero_in  = {.signal = x};
        detect_zero_crossing_params_t zero_params;
        detect_zero_crossing_state_t  zero_state = {.prev_sample = 0.0f};
        detect_zero_crossing_process(&zero_out, &zero_in, &zero_params, &zero_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i] - 1.0f) > 1e-7f)
                return fail("detect_zero_crossing_process mismatch");
        }
        if (fabsf(zero_state.prev_sample - x[frames - 1]) > 1e-7f)
            return fail("detect_zero_crossing_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_zero_crossing_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 1.0f;
            y[i] = -99.0f;
        }
        memset(buffer, 0, sizeof(buffer));
        detect_rms_out_t    rms_out    = {.level = y};
        detect_rms_in_t     rms_in     = {.signal = x};
        detect_rms_params_t rms_params = {.window_size = 4};
        detect_rms_state_t  rms_state  = {.buffer = buffer, .write_pos = 0, .sum = 0.0f};
        detect_rms_process(&rms_out, &rms_in, &rms_params, &rms_state, &info);
        for (int i = 0; i < frames; i++) {
            float expected = sqrtf((float)((i < 4) ? (i + 1) : 4) / 4.0f);
            if (fabsf(y[i] - expected) > 1e-7f)
                return fail("detect_rms_process mismatch");
        }
        if (rms_state.write_pos != frames % 4 || fabsf(rms_state.sum - 4.0f) > 1e-7f)
            return fail("detect_rms_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_rms_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 1.0f;
            y[i] = -99.0f;
        }
        memset(autocorr_buffer, 0, sizeof(autocorr_buffer));
        detect_autocorrelate_out_t    autocorr_out    = {.correlation = y};
        detect_autocorrelate_in_t     autocorr_in     = {.signal = x};
        detect_autocorrelate_params_t autocorr_params = {.max_lag = 8};
        detect_autocorrelate_state_t  autocorr_state  = {.buffer = autocorr_buffer, .write_pos = 0};
        detect_autocorrelate_process(&autocorr_out, &autocorr_in, &autocorr_params, &autocorr_state, &info);
        for (int k = 0; k < 8; k++) {
            float expected = (frames == 1024) ? (float)frames : (float)(frames - k);
            if (fabsf(y[k] - expected) > 1e-7f)
                return fail("detect_autocorrelate_process mismatch");
        }
        if (fabsf(y[8]) > 1e-7f)
            return fail("detect_autocorrelate_process did not clear tail");
        if (autocorr_state.write_pos != frames % (int)APG_DETECT_AUTOCORRELATION_CAPACITY)
            return fail("detect_autocorrelate_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_autocorrelate_process wrote past info.frames");

        for (int i = 0; i < 1024; i++) {
            x[i] = 0.0f;
            y[i] = -99.0f;
        }
        memset(pitch_buffer, 0, sizeof(pitch_buffer));
        detect_pitch_out_t    pitch_out    = {.pitch = y};
        detect_pitch_in_t     pitch_in     = {.signal = x};
        detect_pitch_params_t pitch_params = {.max_lag = 128, .sample_rate = 48000.0f};
        detect_pitch_state_t  pitch_state  = {.buffer = pitch_buffer, .write_pos = 0};
        detect_pitch_process(&pitch_out, &pitch_in, &pitch_params, &pitch_state, &info);
        for (int i = 0; i < frames; i++) {
            if (fabsf(y[i]) > 1e-7f)
                return fail("detect_pitch_process quiet mismatch");
        }
        if (pitch_state.write_pos != frames % (int)APG_DETECT_AUTOCORRELATION_CAPACITY)
            return fail("detect_pitch_process state mismatch");
        if (frames < 1024 && y[frames] != -99.0f)
            return fail("detect_pitch_process wrote past info.frames");
    }

    return test_detector_safety_and_pitch_accuracy();
}
