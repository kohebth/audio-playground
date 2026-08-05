#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define IO_TEST_PI               3.14159265358979323846
#define DEFAULT_RATE             48000U
#define DEFAULT_LATENCY_FRAMES   128U
#define DEFAULT_AMPLITUDE        0.12
#define DEFAULT_TIMEOUT_SECONDS  2.0
#define CHIRP_SECONDS            0.040
#define CAPTURE_WARMUP_SECONDS   0.30
#define PLAYBACK_PREROLL_SECONDS 0.20
#define MATCH_STRIDE             4U

typedef struct {
    uint32_t    rate;
    uint32_t    latency_frames;
    double      amplitude;
    double      timeout_seconds;
    const char *input_target;
    const char *output_target;
    bool        self_test;
} io_test_config_t;

typedef struct {
    size_t end_sample;
    double read_time;
} capture_stamp_t;

typedef struct {
    int              fd;
    float           *samples;
    size_t           sample_capacity;
    size_t           sample_count;
    capture_stamp_t *stamps;
    size_t           stamp_capacity;
    size_t           stamp_count;
    int              read_error;
} capture_state_t;

typedef struct {
    size_t sample;
    double score;
} match_result_t;

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static void sleep_seconds(double seconds) {
    struct timespec delay = {
        .tv_sec  = (time_t)seconds,
        .tv_nsec = (long)((seconds - floor(seconds)) * 1000000000.0),
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static void usage(const char *program) {
    fprintf(
        stderr,
        "Usage: %s [options]\n"
        "\n"
        "Emit a chirp through PipeWire and detect it on the selected input.\n"
        "Use a physical loopback cable for an OS/device round-trip measurement,\n"
        "or a speaker and microphone for an acoustic round-trip measurement.\n"
        "\n"
        "Options:\n"
        "  --rate HZ          Sample rate (default: %u)\n"
        "  --latency FRAMES   Requested pw-cat latency (default: %u)\n"
        "  --amplitude VALUE  Chirp amplitude, 0.01..0.50 (default: %.2f)\n"
        "  --timeout SECONDS  Capture time after chirp (default: %.1f)\n"
        "  --input TARGET     PipeWire input node serial or name\n"
        "  --output TARGET    PipeWire output node serial or name\n"
        "  --self-test        Test detection without opening audio devices\n"
        "  --help             Show this help\n",
        program, DEFAULT_RATE, DEFAULT_LATENCY_FRAMES, DEFAULT_AMPLITUDE, DEFAULT_TIMEOUT_SECONDS
    );
}

static bool parse_u32(const char *text, uint32_t *value) {
    char *end            = NULL;
    errno                = 0;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_double_range(const char *text, double minimum, double maximum, double *value) {
    char *end     = NULL;
    errno         = 0;
    double parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed) || parsed < minimum || parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

static int parse_arguments(int argc, char **argv, io_test_config_t *config) {
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        if (strcmp(argument, "--help") == 0) {
            usage(argv[0]);
            return 1;
        }
        if (strcmp(argument, "--self-test") == 0) {
            config->self_test = true;
            continue;
        }
        if (index + 1 >= argc) {
            fprintf(stderr, "io-test: missing value for %s\n", argument);
            return -1;
        }
        const char *value = argv[++index];
        if (strcmp(argument, "--rate") == 0) {
            if (!parse_u32(value, &config->rate) || config->rate < 8000U || config->rate > 192000U) {
                fprintf(stderr, "io-test: --rate must be between 8000 and 192000\n");
                return -1;
            }
        } else if (strcmp(argument, "--latency") == 0) {
            if (!parse_u32(value, &config->latency_frames) || config->latency_frames > 8192U) {
                fprintf(stderr, "io-test: --latency must be between 1 and 8192 frames\n");
                return -1;
            }
        } else if (strcmp(argument, "--amplitude") == 0) {
            if (!parse_double_range(value, 0.01, 0.50, &config->amplitude)) {
                fprintf(stderr, "io-test: --amplitude must be between 0.01 and 0.50\n");
                return -1;
            }
        } else if (strcmp(argument, "--timeout") == 0) {
            if (!parse_double_range(value, 0.25, 10.0, &config->timeout_seconds)) {
                fprintf(stderr, "io-test: --timeout must be between 0.25 and 10 seconds\n");
                return -1;
            }
        } else if (strcmp(argument, "--input") == 0) {
            config->input_target = value;
        } else if (strcmp(argument, "--output") == 0) {
            config->output_target = value;
        } else {
            fprintf(stderr, "io-test: unknown option: %s\n", argument);
            return -1;
        }
    }
    return 0;
}

static void make_chirp(float *samples, size_t frame_count, uint32_t rate, double amplitude) {
    const double start_frequency = 700.0;
    const double end_frequency   = fmin(9000.0, (double)rate * 0.40);
    double       phase           = 0.0;

    for (size_t frame = 0; frame < frame_count; ++frame) {
        double position  = frame_count > 1 ? (double)frame / (double)(frame_count - 1) : 0.0;
        double frequency = start_frequency + (end_frequency - start_frequency) * position;
        double window    = 0.5 - 0.5 * cos(2.0 * IO_TEST_PI * position);
        samples[frame]   = (float)(amplitude * window * sin(phase));
        phase += 2.0 * IO_TEST_PI * frequency / (double)rate;
    }
}

static double
correlation_score(const float *captured, const float *reference, size_t reference_count, size_t sample_stride) {
    double dot              = 0.0;
    double captured_energy  = 0.0;
    double reference_energy = 0.0;
    for (size_t index = 0; index < reference_count; index += sample_stride) {
        double input    = captured[index];
        double expected = reference[index];
        dot += input * expected;
        captured_energy += input * input;
        reference_energy += expected * expected;
    }
    if (captured_energy < 1e-12 || reference_energy < 1e-12) {
        return 0.0;
    }
    return fabs(dot) / sqrt(captured_energy * reference_energy);
}

static match_result_t find_chirp(const float *captured, size_t captured_count, const float *chirp, size_t chirp_count) {
    match_result_t best = {0, 0.0};
    if (captured_count < chirp_count) {
        return best;
    }

    size_t last_start = captured_count - chirp_count;
    for (size_t start = 0; start <= last_start; start += MATCH_STRIDE) {
        double score = correlation_score(captured + start, chirp, chirp_count, MATCH_STRIDE);
        if (score > best.score) {
            best.sample = start;
            best.score  = score;
        }
    }

    size_t refine_begin = best.sample > MATCH_STRIDE ? best.sample - MATCH_STRIDE : 0;
    size_t refine_end   = best.sample + MATCH_STRIDE < last_start ? best.sample + MATCH_STRIDE : last_start;
    for (size_t start = refine_begin; start <= refine_end; ++start) {
        double score = correlation_score(captured + start, chirp, chirp_count, 1);
        if (score > best.score) {
            best.sample = start;
            best.score  = score;
        }
    }
    return best;
}

static bool append_stamp(capture_state_t *capture, size_t end_sample, double read_time) {
    if (capture->stamp_count == capture->stamp_capacity) {
        size_t           next_capacity = capture->stamp_capacity == 0 ? 64 : capture->stamp_capacity * 2;
        capture_stamp_t *next          = realloc(capture->stamps, next_capacity * sizeof(*next));
        if (next == NULL) {
            return false;
        }
        capture->stamps         = next;
        capture->stamp_capacity = next_capacity;
    }
    capture->stamps[capture->stamp_count++] = (capture_stamp_t){.end_sample = end_sample, .read_time = read_time};
    return true;
}

static void *capture_thread(void *opaque) {
    capture_state_t *capture = opaque;
    while (capture->sample_count < capture->sample_capacity) {
        size_t  remaining     = capture->sample_capacity - capture->sample_count;
        size_t  chunk_samples = remaining < 1024U ? remaining : 1024U;
        ssize_t bytes = read(capture->fd, capture->samples + capture->sample_count, chunk_samples * sizeof(float));
        if (bytes > 0) {
            size_t sample_count = (size_t)bytes / sizeof(float);
            capture->sample_count += sample_count;
            if (!append_stamp(capture, capture->sample_count, monotonic_seconds())) {
                capture->read_error = ENOMEM;
                break;
            }
            continue;
        }
        if (bytes == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        capture->read_error = errno;
        break;
    }
    close(capture->fd);
    return NULL;
}

static pid_t spawn_pw_cat(bool record, int audio_fd, const io_test_config_t *config) {
    pid_t child = fork();
    if (child != 0) {
        return child;
    }

    if (record) {
        if (dup2(audio_fd, STDOUT_FILENO) < 0) {
            _exit(126);
        }
    } else if (dup2(audio_fd, STDIN_FILENO) < 0) {
        _exit(126);
    }
    close(audio_fd);

    char rate[32];
    char latency[32];
    snprintf(rate, sizeof(rate), "%u", config->rate);
    snprintf(latency, sizeof(latency), "%u", config->latency_frames);

    char  *arguments[20];
    size_t count       = 0;
    arguments[count++] = "pw-cat";
    arguments[count++] = record ? "--record" : "--playback";
    arguments[count++] = "--rate";
    arguments[count++] = rate;
    arguments[count++] = "--channels";
    arguments[count++] = "1";
    arguments[count++] = "--channel-map";
    arguments[count++] = "mono";
    arguments[count++] = "--format";
    arguments[count++] = "f32";
    arguments[count++] = "--latency";
    arguments[count++] = latency;
    const char *target = record ? config->input_target : config->output_target;
    if (target != NULL) {
        arguments[count++] = "--target";
        arguments[count++] = (char *)target;
    }
    arguments[count++] = "-";
    arguments[count]   = NULL;
    execvp(arguments[0], arguments);
    fprintf(stderr, "io-test: cannot start pw-cat: %s\n", strerror(errno));
    _exit(127);
}

static bool write_all(int fd, const void *data, size_t byte_count) {
    const uint8_t *cursor = data;
    while (byte_count > 0) {
        ssize_t written = write(fd, cursor, byte_count);
        if (written > 0) {
            cursor += written;
            byte_count -= (size_t)written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

static bool make_cloexec_pipe(int descriptors[2]) {
    if (pipe(descriptors) != 0) {
        return false;
    }
    for (size_t index = 0; index < 2; ++index) {
        int flags = fcntl(descriptors[index], F_GETFD);
        if (flags < 0 || fcntl(descriptors[index], F_SETFD, flags | FD_CLOEXEC) < 0) {
            int error = errno;
            close(descriptors[0]);
            close(descriptors[1]);
            errno = error;
            return false;
        }
    }
    return true;
}

static bool write_paced_silence(int fd, size_t frame_count, const io_test_config_t *config) {
    size_t block_count = config->latency_frames;
    float *silence     = calloc(block_count, sizeof(*silence));
    if (silence == NULL) {
        return false;
    }

    size_t written_frames = 0;
    while (written_frames < frame_count) {
        size_t current = frame_count - written_frames;
        if (current > block_count) {
            current = block_count;
        }
        if (!write_all(fd, silence, current * sizeof(*silence))) {
            free(silence);
            return false;
        }
        written_frames += current;
        sleep_seconds((double)current / (double)config->rate);
    }
    free(silence);
    return true;
}

static double capture_time_for_sample(const capture_state_t *capture, size_t sample, uint32_t rate) {
    for (size_t index = 0; index < capture->stamp_count; ++index) {
        if (sample < capture->stamps[index].end_sample) {
            size_t samples_before_end = capture->stamps[index].end_sample - sample;
            return capture->stamps[index].read_time - (double)samples_before_end / (double)rate;
        }
    }
    return NAN;
}

static void terminate_child(pid_t child) {
    if (child <= 0) {
        return;
    }
    if (waitpid(child, NULL, WNOHANG) == 0) {
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
    }
}

static int run_self_test(void) {
    const uint32_t rate          = DEFAULT_RATE;
    const size_t   chirp_count   = (size_t)llround(CHIRP_SECONDS * rate);
    const size_t   delay         = 4321U;
    const size_t   capture_count = delay + chirp_count + 2048U;
    float         *chirp         = calloc(chirp_count, sizeof(*chirp));
    float         *capture       = calloc(capture_count, sizeof(*capture));
    if (chirp == NULL || capture == NULL) {
        free(chirp);
        free(capture);
        return 1;
    }

    make_chirp(chirp, chirp_count, rate, 0.2);
    uint32_t noise = 0x12345678U;
    for (size_t index = 0; index < capture_count; ++index) {
        noise          = noise * 1664525U + 1013904223U;
        capture[index] = ((float)((noise >> 8U) & 0xffffU) / 32768.0f - 1.0f) * 0.002f;
    }
    for (size_t index = 0; index < chirp_count; ++index) {
        capture[delay + index] += chirp[index] * 0.35f;
    }

    match_result_t match = find_chirp(capture, capture_count, chirp, chirp_count);
    free(chirp);
    free(capture);
    if (match.sample != delay || match.score < 0.95) {
        fprintf(
            stderr, "io-test self-test failed: expected %zu, found %zu (score %.3f)\n", delay, match.sample, match.score
        );
        return 1;
    }
    printf("io-test self-test passed: sample=%zu score=%.3f\n", match.sample, match.score);
    return 0;
}

static int run_audio_test(const io_test_config_t *config) {
    size_t chirp_count      = (size_t)llround(CHIRP_SECONDS * config->rate);
    size_t capture_capacity = (size_t
    )llround((CAPTURE_WARMUP_SECONDS + PLAYBACK_PREROLL_SECONDS + config->timeout_seconds + 0.5) * config->rate);
    float *chirp            = calloc(chirp_count, sizeof(*chirp));
    float *captured         = calloc(capture_capacity, sizeof(*captured));
    if (chirp == NULL || captured == NULL) {
        fprintf(stderr, "io-test: out of memory\n");
        free(chirp);
        free(captured);
        return 1;
    }
    make_chirp(chirp, chirp_count, config->rate, config->amplitude);

    int capture_pipe[2];
    int playback_pipe[2];
    if (!make_cloexec_pipe(capture_pipe)) {
        fprintf(stderr, "io-test: cannot create audio pipes: %s\n", strerror(errno));
        free(chirp);
        free(captured);
        return 1;
    }
    if (!make_cloexec_pipe(playback_pipe)) {
        fprintf(stderr, "io-test: cannot create audio pipes: %s\n", strerror(errno));
        close(capture_pipe[0]);
        close(capture_pipe[1]);
        free(chirp);
        free(captured);
        return 1;
    }

    pid_t recorder = spawn_pw_cat(true, capture_pipe[1], config);
    if (recorder < 0) {
        fprintf(stderr, "io-test: cannot start recorder: %s\n", strerror(errno));
        return 1;
    }
    close(capture_pipe[1]);

    capture_state_t capture = {
        .fd              = capture_pipe[0],
        .samples         = captured,
        .sample_capacity = capture_capacity,
    };
    pthread_t capture_worker;
    if (pthread_create(&capture_worker, NULL, capture_thread, &capture) != 0) {
        fprintf(stderr, "io-test: cannot start capture thread\n");
        terminate_child(recorder);
        return 1;
    }

    printf(
        "Capturing at %u Hz; requested PipeWire quantum: %u frames (%.3f ms)\n", config->rate, config->latency_frames,
        1000.0 * config->latency_frames / config->rate
    );
    printf("Chirp amplitude: %.2f. Keep monitoring volume low.\n", config->amplitude);
    fflush(stdout);
    sleep_seconds(CAPTURE_WARMUP_SECONDS);

    pid_t player = spawn_pw_cat(false, playback_pipe[0], config);
    close(playback_pipe[0]);
    if (player < 0) {
        fprintf(stderr, "io-test: cannot start player: %s\n", strerror(errno));
        close(playback_pipe[1]);
        terminate_child(recorder);
        pthread_join(capture_worker, NULL);
        return 1;
    }

    size_t preroll_frames = (size_t)llround(PLAYBACK_PREROLL_SECONDS * config->rate);
    if (!write_paced_silence(playback_pipe[1], preroll_frames, config)) {
        fprintf(stderr, "io-test: playback preroll failed: %s\n", strerror(errno));
        close(playback_pipe[1]);
        terminate_child(player);
        terminate_child(recorder);
        pthread_join(capture_worker, NULL);
        return 1;
    }

    double chirp_submit_time = monotonic_seconds();
    if (!write_all(playback_pipe[1], chirp, chirp_count * sizeof(*chirp))) {
        fprintf(stderr, "io-test: chirp playback failed: %s\n", strerror(errno));
    }
    write_paced_silence(playback_pipe[1], config->rate / 10U, config);
    close(playback_pipe[1]);
    waitpid(player, NULL, 0);

    sleep_seconds(config->timeout_seconds);
    terminate_child(recorder);
    pthread_join(capture_worker, NULL);

    int result = 0;
    if (capture.read_error != 0) {
        fprintf(stderr, "io-test: capture failed: %s\n", strerror(capture.read_error));
        result = 1;
    } else {
        match_result_t match         = find_chirp(capture.samples, capture.sample_count, chirp, chirp_count);
        double         detected_time = capture_time_for_sample(&capture, match.sample, config->rate);
        double         latency_ms    = (detected_time - chirp_submit_time) * 1000.0;
        printf("Captured samples: %zu\n", capture.sample_count);
        printf("Detection score: %.3f\n", match.score);
        if (match.score < 0.20 || !isfinite(latency_ms) || latency_ms < 0.0) {
            fprintf(
                stderr, "No reliable chirp detected. Check routing/loopback, then increase --amplitude carefully.\n"
            );
            result = 2;
        } else {
            printf(
                "Client-observed round-trip latency: %.3f ms (chirp at capture frame %zu)\n", latency_ms, match.sample
            );
            printf("Note: result includes pw-cat pipe scheduling; repeat the test and use the lowest stable reading.\n"
            );
        }
    }

    free(capture.stamps);
    free(captured);
    free(chirp);
    return result;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    io_test_config_t config = {
        .rate            = DEFAULT_RATE,
        .latency_frames  = DEFAULT_LATENCY_FRAMES,
        .amplitude       = DEFAULT_AMPLITUDE,
        .timeout_seconds = DEFAULT_TIMEOUT_SECONDS,
    };
    int parsed = parse_arguments(argc, argv, &config);
    if (parsed != 0) {
        return parsed > 0 ? 0 : 2;
    }
    if (config.self_test) {
        return run_self_test();
    }
    return run_audio_test(&config);
}
