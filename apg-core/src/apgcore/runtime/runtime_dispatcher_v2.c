#include <apgcore/runtime/runtime_v2_internal.h>

#include <string.h>

typedef enum {
    APG_RUNTIME_PROCESS_BEGIN,
    APG_RUNTIME_PROCESS_RESOLVE_PORTS,
    APG_RUNTIME_PROCESS_IMPORT_INPUT,
    APG_RUNTIME_PROCESS_PREPARE_BLOCK,
    APG_RUNTIME_PROCESS_RUN_NODE,
    APG_RUNTIME_PROCESS_APPLY_MUTE,
    APG_RUNTIME_PROCESS_EXPORT_OUTPUT,
    APG_RUNTIME_PROCESS_DONE,
    APG_RUNTIME_PROCESS_FAIL,
} apg_runtime_process_state_t;

typedef enum {
    APG_RUNTIME_PROCESS_IO_NONE,
    APG_RUNTIME_PROCESS_IO_MONO,
    APG_RUNTIME_PROCESS_IO_INTERLEAVED,
} apg_runtime_process_io_t;

typedef struct {
    apg_v2_runtime_t                   *runtime;
    uint32_t                            frames;
    apg_runtime_process_state_t         state;
    apg_runtime_process_io_t            io;
    const apg_v2_registry_audio_port_t *input_port;
    const apg_v2_registry_audio_port_t *output_port;
    const float                        *input;
    float                              *output;
    size_t                              input_channels;
    size_t                              output_channels;
    size_t                              schedule_cursor;
} apg_runtime_process_context_t;

static bool process_context_fail(apg_runtime_process_context_t *ctx, const char *msg) {
    if (ctx && ctx->runtime && msg)
        apg_v2_runtime_set_error(ctx->runtime, msg);
    if (ctx)
        ctx->state = APG_RUNTIME_PROCESS_FAIL;
    return false;
}

static bool process_resolve_ports(apg_runtime_process_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;

    if (ctx->io == APG_RUNTIME_PROCESS_IO_NONE) {
        ctx->state = APG_RUNTIME_PROCESS_IMPORT_INPUT;
        return true;
    }

    if (ctx->io == APG_RUNTIME_PROCESS_IO_MONO) {
        if (!ctx->input || !ctx->output)
            return process_context_fail(ctx, "v2 runtime mono input/output buffers are required");
        if (!ctx->input_port || ctx->input_port->channel_count == 0u ||
            ctx->input_port->signal_indices[0] >= ctx->runtime->signals_len)
            return process_context_fail(ctx, "v2 runtime input audio port signal lookup failed");
        if (!ctx->output_port || ctx->output_port->channel_count == 0u ||
            ctx->output_port->signal_indices[0] >= ctx->runtime->signals_len)
            return process_context_fail(ctx, "v2 runtime output audio port signal lookup failed");
        if (ctx->input_port->channel_count != 1u || ctx->output_port->channel_count != 1u)
            return process_context_fail(ctx, "v2 runtime mono processing requires mono audio ports");
        ctx->input_channels  = 1u;
        ctx->output_channels = 1u;
        ctx->state           = APG_RUNTIME_PROCESS_IMPORT_INPUT;
        return true;
    }

    if (!ctx->input || !ctx->output)
        return process_context_fail(ctx, "v2 runtime interleaved input/output buffers are required");
    if (!ctx->input_port || ctx->input_port->channel_count == 0u || !ctx->input_port->signal_indices)
        return process_context_fail(ctx, "v2 runtime input audio port signal lookup failed");
    if (!ctx->output_port || ctx->output_port->channel_count == 0u || !ctx->output_port->signal_indices)
        return process_context_fail(ctx, "v2 runtime output audio port signal lookup failed");

    ctx->input_channels  = ctx->input_port->channel_count;
    ctx->output_channels = ctx->output_port->channel_count;
    for (size_t ch = 0; ch < ctx->input_channels; ch++) {
        if (ctx->input_port->signal_indices[ch] >= ctx->runtime->signals_len)
            return process_context_fail(ctx, "v2 runtime input audio port signal lookup failed");
    }
    for (size_t ch = 0; ch < ctx->output_channels; ch++) {
        if (ctx->output_port->signal_indices[ch] >= ctx->runtime->signals_len)
            return process_context_fail(ctx, "v2 runtime output audio port signal lookup failed");
    }

    ctx->state = APG_RUNTIME_PROCESS_IMPORT_INPUT;
    return true;
}

static bool process_import_input(apg_runtime_process_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;

    if (ctx->io == APG_RUNTIME_PROCESS_IO_MONO) {
        memcpy(ctx->runtime->signals[ctx->input_port->signal_indices[0]], ctx->input, ctx->frames * sizeof(float));
    } else if (ctx->io == APG_RUNTIME_PROCESS_IO_INTERLEAVED) {
        for (size_t ch = 0; ch < ctx->input_channels; ch++) {
            size_t index = ctx->input_port->signal_indices[ch];
            for (uint32_t frame = 0; frame < ctx->frames; frame++)
                ctx->runtime->signals[index][frame] = ctx->input[(size_t)frame * ctx->input_channels + ch];
        }
    }

    ctx->state = APG_RUNTIME_PROCESS_PREPARE_BLOCK;
    return true;
}

static bool process_prepare_block(apg_runtime_process_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;
    ctx->runtime->process_context.frames = ctx->frames;
    apg_v2_runtime_advance_smoothed_params(ctx->runtime, ctx->frames);
    ctx->schedule_cursor = 0u;
    ctx->state           = APG_RUNTIME_PROCESS_RUN_NODE;
    return true;
}

static bool process_run_node(apg_runtime_process_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;
    if (ctx->schedule_cursor >= ctx->runtime->schedule_len) {
        ctx->state = APG_RUNTIME_PROCESS_APPLY_MUTE;
        return true;
    }
    size_t node_index = ctx->runtime->schedule[ctx->schedule_cursor++];
    if (!apg_v2_runtime_run_node(ctx->runtime, node_index, ctx->frames)) {
        ctx->state = APG_RUNTIME_PROCESS_FAIL;
        return false;
    }
    return true;
}

static bool process_export_output(apg_runtime_process_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;

    if (ctx->io == APG_RUNTIME_PROCESS_IO_MONO) {
        memcpy(ctx->output, ctx->runtime->signals[ctx->output_port->signal_indices[0]], ctx->frames * sizeof(float));
    } else if (ctx->io == APG_RUNTIME_PROCESS_IO_INTERLEAVED) {
        for (size_t ch = 0; ch < ctx->output_channels; ch++) {
            size_t index = ctx->output_port->signal_indices[ch];
            for (uint32_t frame = 0; frame < ctx->frames; frame++)
                ctx->output[(size_t)frame * ctx->output_channels + ch] = ctx->runtime->signals[index][frame];
        }
    }
    ctx->runtime->has_processed = true;
    if (ctx->runtime->process_context.sample_position <= UINT64_MAX - ctx->frames)
        ctx->runtime->process_context.sample_position += ctx->frames;
    else
        ctx->runtime->process_context.sample_position = UINT64_MAX;
    ctx->state = APG_RUNTIME_PROCESS_DONE;
    return true;
}

static bool process_dispatch(apg_runtime_process_context_t *ctx) {
    if (!ctx || !ctx->runtime)
        return false;

    while (ctx->state != APG_RUNTIME_PROCESS_DONE && ctx->state != APG_RUNTIME_PROCESS_FAIL) {
        switch (ctx->state) {
        case APG_RUNTIME_PROCESS_BEGIN:
            ctx->runtime->last_error[0] = '\0';
            if (!apg_v2_runtime_execution_metadata_ready(ctx->runtime))
                return process_context_fail(ctx, "v2 registry execution metadata is missing");
            if (ctx->frames == 0u)
                return process_context_fail(ctx, "v2 runtime frame count must be greater than zero");
            if (ctx->frames > ctx->runtime->frame_capacity)
                return process_context_fail(ctx, "v2 runtime frame count exceeds capacity");
            ctx->state = ctx->io == APG_RUNTIME_PROCESS_IO_NONE ? APG_RUNTIME_PROCESS_PREPARE_BLOCK
                                                                : APG_RUNTIME_PROCESS_RESOLVE_PORTS;
            break;
        case APG_RUNTIME_PROCESS_RESOLVE_PORTS:
            if (!process_resolve_ports(ctx))
                return false;
            break;
        case APG_RUNTIME_PROCESS_IMPORT_INPUT:
            if (!process_import_input(ctx))
                return false;
            break;
        case APG_RUNTIME_PROCESS_PREPARE_BLOCK:
            if (!process_prepare_block(ctx))
                return false;
            break;
        case APG_RUNTIME_PROCESS_RUN_NODE:
            if (!process_run_node(ctx))
                return false;
            break;
        case APG_RUNTIME_PROCESS_APPLY_MUTE:
            apg_v2_runtime_apply_project_mute(ctx->runtime, ctx->frames);
            ctx->state = APG_RUNTIME_PROCESS_EXPORT_OUTPUT;
            break;
        case APG_RUNTIME_PROCESS_EXPORT_OUTPUT:
            if (!process_export_output(ctx))
                return false;
            break;
        case APG_RUNTIME_PROCESS_DONE:
        case APG_RUNTIME_PROCESS_FAIL:
            break;
        }
    }

    return ctx->state == APG_RUNTIME_PROCESS_DONE;
}

bool apg_v2_runtime_dispatch_process(apg_v2_runtime_t *runtime, uint32_t frames) {
    apg_runtime_process_context_t ctx = {
        .runtime = runtime,
        .frames  = frames,
        .state   = APG_RUNTIME_PROCESS_BEGIN,
        .io      = APG_RUNTIME_PROCESS_IO_NONE,
    };
    return process_dispatch(&ctx);
}

bool apg_v2_runtime_dispatch_process_interleaved_ports(
    apg_v2_runtime_t                   *runtime,
    const apg_v2_registry_audio_port_t *input_port,
    const float                        *input,
    const apg_v2_registry_audio_port_t *output_port,
    float                              *output,
    uint32_t                            frames
) {
    apg_runtime_process_context_t ctx = {
        .runtime     = runtime,
        .frames      = frames,
        .state       = APG_RUNTIME_PROCESS_BEGIN,
        .io          = APG_RUNTIME_PROCESS_IO_INTERLEAVED,
        .input_port  = input_port,
        .output_port = output_port,
        .input       = input,
        .output      = output,
    };
    return process_dispatch(&ctx);
}

bool apg_v2_runtime_dispatch_process_mono_audio_ports(
    apg_v2_runtime_t                   *runtime,
    const apg_v2_registry_audio_port_t *input_port,
    const float                        *input,
    const apg_v2_registry_audio_port_t *output_port,
    float                              *output,
    uint32_t                            frames
) {
    apg_runtime_process_context_t ctx = {
        .runtime     = runtime,
        .frames      = frames,
        .state       = APG_RUNTIME_PROCESS_BEGIN,
        .io          = APG_RUNTIME_PROCESS_IO_MONO,
        .input_port  = input_port,
        .output_port = output_port,
        .input       = input,
        .output      = output,
    };
    return process_dispatch(&ctx);
}
