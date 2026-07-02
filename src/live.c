#include <apgcore/host_v2.h>
#include <util/fast_chunk.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_COUNT  16
#define CHUNK_LENGTH 512
#define MAX_UNITS    8
#define SAMPLE_RATE  48000

struct IOBuffers {
    LiveChunk        chunks[16];
    volatile uint8_t write_idx;
    volatile uint8_t read_idx;
};

struct AudioNode {
    struct pw_main_loop *loop;
    struct pw_stream    *stream;
    struct IOBuffers    *buffer;
    apg_v2_host_unit_t  *units;
    int                  n_units;
    int                  max_units;
};

static inline bool is_not_null(void *ptr) { return ptr != NULL; }

static void on_capture_process(void *userdata) {
    struct AudioNode *node = (struct AudioNode *)(userdata);
    struct pw_buffer *b;

    if ((b = pw_stream_dequeue_buffer(node->stream)) == NULL) {
        return;
    }

    const struct spa_buffer *buf = b->buffer;

    if (is_not_null(buf->datas[0].data) && buf->datas[0].chunk->size > 0) {
        uint32_t          size  = buf->datas[0].chunk->size;
        volatile uint8_t *idx   = &node->buffer->write_idx;
        LiveChunk        *chunk = &node->buffer->chunks[*idx];

        uint32_t max_bytes = CHUNK_LENGTH * sizeof(float);
        if (size > max_bytes)
            size = max_bytes;

        // Write to buffer if not in use
        if (!chunk->ready) {
            memcpy(chunk->data, buf->datas[0].data, size);
            chunk->length = size / sizeof(float);
            __sync_synchronize(); // Memory barrier
            chunk->ready = 1;
            *idx         = (*idx + 1) & (CHUNK_COUNT - 1);
        }
    }

    pw_stream_queue_buffer(node->stream, b);
}

static void on_playback_process(void *userdata) {
    struct AudioNode *node = (struct AudioNode *)(userdata);
    struct pw_buffer *b;

    if ((b = pw_stream_dequeue_buffer(node->stream)) == NULL) {
        return;
    }

    const struct spa_buffer *buf = b->buffer;

    if (is_not_null(buf->datas[0].data)) {
        uint32_t size = CHUNK_LENGTH;
        if (size * sizeof(float) > buf->datas[0].maxsize) {
            size = buf->datas[0].maxsize / sizeof(float);
        }

        float output_data[CHUNK_LENGTH];
        memset(output_data, 0, sizeof(output_data));

        volatile uint8_t *read_idx  = &node->buffer->read_idx;
        volatile uint8_t *write_idx = &node->buffer->write_idx;
        LiveChunk        *chunk     = &node->buffer->chunks[*read_idx];

        if (chunk->ready) {
            uint32_t chunk_size = chunk->length;
            if (chunk_size > size)
                chunk_size = size;

            if (node->n_units > 0) {
                float        stage_a[CHUNK_LENGTH];
                float        stage_b[CHUNK_LENGTH];
                const float *current_in = chunk->data;
                bool         processed  = true;
                for (int i = 0; i < node->n_units; i++) {
                    float *current_out = (current_in == stage_a) ? stage_b : stage_a;
                    if (!apg_v2_host_process_mono_ports(
                            &node->units[i], "input", current_in, "output", current_out, chunk_size
                        )) {
                        memset(output_data, 0, chunk_size * sizeof(float));
                        processed = false;
                        break;
                    }
                    current_in = current_out;
                }
                if (processed)
                    memcpy(output_data, current_in, chunk_size * sizeof(float));
            } else {
                memcpy(output_data, chunk->data, chunk_size * sizeof(float));
            }

            __sync_synchronize();
            chunk->ready = 0;
            *read_idx    = (*read_idx + 1) & (CHUNK_COUNT - 1);
        }

        memcpy(buf->datas[0].data, output_data, size * sizeof(float));
        buf->datas[0].chunk->size   = size * sizeof(float);
        buf->datas[0].chunk->stride = 4;
    }

    pw_stream_queue_buffer(node->stream, b);
}

static const struct pw_stream_events capture_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_capture_process,
};

static const struct pw_stream_events playback_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_playback_process,
};

static void do_quit(void *loop, int signal_number) { pw_main_loop_quit(loop); }

int main(int argc, char *argv[]) {
    const struct spa_pod  *params[1];
    uint8_t                buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    pw_init(&argc, &argv);
    struct pw_main_loop *loop = pw_main_loop_new(NULL);
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGINT, do_quit, loop);
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGTERM, do_quit, loop);

    struct IOBuffers io_buffer = {.chunks = {0}, .write_idx = 0, .read_idx = 0};

    // Create capture stream (microphone)
    struct AudioNode capture = {
        .loop   = loop,
        .stream = pw_stream_new_simple(
            pw_main_loop_get_loop(loop), "n!audio-capture",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Music",
                PW_KEY_NODE_LATENCY, "512/48000", PW_KEY_NODE_RATE, "1/48000", NULL
            ),
            &capture_events, &capture
        ),
        .buffer    = &io_buffer,
        .units     = NULL,
        .n_units   = 0,
        .max_units = MAX_UNITS
    };

    apg_v2_host_unit_t *playback_units = calloc(MAX_UNITS, sizeof(apg_v2_host_unit_t));
    struct AudioNode    playback       = {
                 .loop   = loop,
                 .stream = pw_stream_new_simple(
            pw_main_loop_get_loop(loop), "n!audio-playback",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Music",
                PW_KEY_NODE_LATENCY, "512/48000", PW_KEY_NODE_RATE, "1/48000", NULL
            ),
            &playback_events, &playback
        ),
                 .buffer    = &io_buffer,
                 .units     = playback_units,
                 .n_units   = 0,
                 .max_units = MAX_UNITS
    };

    if (argc > 1) {
        for (int i = 1; i < argc && playback.n_units < playback.max_units; i++) {
            printf("Loading v2 unit: %s\n", argv[i]);
            int      unit_index = playback.n_units;
            uc_error err        = {0};
            if (apg_v2_host_load_file(argv[i], CHUNK_LENGTH, (float)SAMPLE_RATE, &playback.units[unit_index], &err) ==
                UC_OK) {
                playback.n_units++;
            } else {
                fprintf(stderr, "Failed to load v2 unit %s: %s\n", argv[i], err.msg);
            }
        }
    } else {
        uc_error err = {0};
        if (apg_v2_host_load_file(
                "../units-v2/simple_gain.unit.v2.yaml", CHUNK_LENGTH, (float)SAMPLE_RATE, &playback.units[0], &err
            ) == UC_OK) {
            playback.n_units = 1;
        } else {
            fprintf(stderr, "Failed to load default v2 unit: %s\n", err.msg);
        }
    }

    // Set audio format: 48kHz, f32, 1 channels (mono)
    struct spa_audio_info_raw audio_info = {
        .format   = SPA_AUDIO_FORMAT_F32,
        .rate     = SAMPLE_RATE,
        .channels = 1,
    };

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect capture stream
    pw_stream_connect(
        capture.stream, PW_DIRECTION_INPUT, PW_ID_ANY,
        (enum pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        params, 1
    );

    // Rebuild params for playback stream
    b         = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect playback stream
    pw_stream_connect(
        playback.stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        (enum pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        params, 1
    );

    printf("Real-time mic to speaker forwarding. Press Ctrl+C to stop.\n");
    printf("Warning: This may cause feedback! Keep volume low or use headphones.\n");
    pw_main_loop_run(loop);

    pw_stream_destroy(capture.stream);
    pw_stream_destroy(playback.stream);
    pw_main_loop_destroy(loop);
    for (int i = 0; i < playback.n_units; i++) {
        apg_v2_host_destroy(&playback.units[i]);
    }
    free(playback_units);
    pw_deinit();

    return 0;
}
