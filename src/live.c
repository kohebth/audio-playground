#include <runtime.h>
#include <util/fast_chunk.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>

#define CHUNK_COUNT 16
#define CHUNK_LENGTH 512
#define BOUND 32767.0

struct IOBuffers {
    LiveChunk chunks[16];
    volatile uint8_t write_idx;
    volatile uint8_t read_idx;
};

struct AudioNode {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct IOBuffers *buffer;
    runtime_unit_t *unit;   // YAML-loaded DSP unit (NULL for capture)
};

inline bool is_not_null(void *ptr) {
    return ptr != NULL;
}

static void on_capture_process(void *userdata) {
    const struct AudioNode *node = (struct AudioNode *)(userdata);
    struct pw_buffer *b;

    if ((b = pw_stream_dequeue_buffer(node->stream)) == NULL) {
        return;
    }

    const struct spa_buffer *buf = b->buffer;

    if (is_not_null(buf->datas[0].data) && buf->datas[0].chunk->size > 0) {
        uint32_t size = buf->datas[0].chunk->size;
        volatile uint8_t *idx = &node->buffer->write_idx;
        LiveChunk *chunk = &node->buffer->chunks[*idx];

        uint32_t max_bytes = CHUNK_LENGTH * sizeof(float);
        if (size > max_bytes) size = max_bytes;

        // Write to buffer if not in use
        if (!chunk->ready) {
            memcpy(chunk->data, buf->datas[0].data, size);
            chunk->length = size/sizeof(float);
            __sync_synchronize(); // Memory barrier
            chunk->ready = 1;
            *idx = (*idx + 1) & (CHUNK_COUNT - 1);
        }
    }

    pw_stream_queue_buffer(node->stream, b);
}

static void on_playback_process(void *userdata) {
    const struct AudioNode *node = (struct AudioNode *)(userdata);
    struct pw_buffer *b;

    if ((b = pw_stream_dequeue_buffer(node->stream)) == NULL) {
        return;
    }

    const struct spa_buffer *buf = b->buffer;

    if (is_not_null(buf->datas[0].data)) {
        volatile uint8_t *idx = &node->buffer->read_idx;
        LiveChunk *chunk = &node->buffer->chunks[*idx];

        // Read from buffer if ready
        if (chunk->ready) {
            uint16_t size = chunk->length;
            if (size > buf->datas[0].maxsize) size = buf->datas[0].maxsize;

            float output_data[CHUNK_LENGTH];

            if (node->unit) {
                runtime_unit_process(node->unit, chunk->data, output_data);
            } else {
                memcpy(output_data, chunk->data, size * sizeof(float));
            }

            // Output
            memcpy(buf->datas[0].data, output_data, size * sizeof(float));
            buf->datas[0].chunk->size = size * sizeof(float);
            buf->datas[0].chunk->stride = 4;

            __sync_synchronize(); // Memory barrier
            chunk->ready = 0;
            *idx = (*idx + 1) & (CHUNK_COUNT - 1);
        }
        // else {
            // Output silence if no data available
            // memset(buf->datas[0].data, 0, buf->datas[0].maxsize);
            // buf->datas[0].chunk->size = buf->datas[0].maxsize;
            // buf->datas[0].chunk->stride = 1;
        // }
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

static void do_quit(void *loop, int signal_number) {
    pw_main_loop_quit(loop);
}


int main(int argc, char *argv[]) {
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    pw_init(&argc, &argv);
    struct pw_main_loop *loop = pw_main_loop_new(NULL);
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGINT, do_quit, loop);
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGTERM, do_quit, loop);

    struct IOBuffers io_buffer = {.chunks = {0}, .write_idx = 0, .read_idx = 0};

    // Load DSP unit from YAML
    runtime_context_t rt_ctx = { .sample_rate = 48000, .chunk_length = CHUNK_LENGTH };
    runtime_unit_t *unit = runtime_unit_load("../units/chorus.unit.yaml", rt_ctx);
    if (!unit) {
        fprintf(stderr, "Failed to load unit YAML\n");
        pw_main_loop_destroy(loop);
        pw_deinit();
        return 1;
    }

    // Create capture stream (microphone)
    struct AudioNode capture = {
        .loop = loop,
        .stream = pw_stream_new_simple(
            pw_main_loop_get_loop(loop),
            "n!audio-capture",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Capture",
                PW_KEY_MEDIA_ROLE, "Music",
                // PW_KEY_TARGET_OBJECT, "alsa_input.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__hw_sofhdadsp__source",
                NULL
            ),
            &capture_events,
            &capture
        ),
        .buffer = &io_buffer
    };

    // Create playback stream (speaker)
    struct AudioNode playback = {
        .loop = loop,
        .stream = pw_stream_new_simple(
            pw_main_loop_get_loop(loop),
            "n!audio-playback",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Playback",
                PW_KEY_MEDIA_ROLE, "Music",
                // PW_KEY_TARGET_OBJECT, "alsa_output.pci-0000_00_1f.3-platform-skl_hda_dsp_generic.HiFi__hw_sofhdadsp__sink",
                NULL
            ),
            &playback_events,
            &playback
        ),
        .buffer = &io_buffer,
        .unit = unit
    };

    // Set audio format: 48kHz, f32, 1 channels (mono)
    struct spa_audio_info_raw audio_info = {
        .format = SPA_AUDIO_FORMAT_F32,
        .rate = 48000,
        .channels = 1,
    };

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect capture stream
    pw_stream_connect(
        capture.stream,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        (enum pw_stream_flags) (
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS |
            PW_STREAM_FLAG_RT_PROCESS
        ),
        params, 1);

    // Rebuild params for playback stream
    b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info);

    // Connect playback stream
    pw_stream_connect(
        playback.stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (enum pw_stream_flags) (
            PW_STREAM_FLAG_AUTOCONNECT |
            PW_STREAM_FLAG_MAP_BUFFERS |
            PW_STREAM_FLAG_RT_PROCESS
        ),
        params, 1);


    printf("Real-time mic to speaker forwarding. Press Ctrl+C to stop.\n");
    printf("Warning: This may cause feedback! Keep volume low or use headphones.\n");
    pw_main_loop_run(loop);

    pw_stream_destroy(capture.stream);
    pw_stream_destroy(playback.stream);
    pw_main_loop_destroy(loop);
    runtime_unit_destroy(unit);
    pw_deinit();

    return 0;
}
