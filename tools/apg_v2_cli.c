#include <apgcore/compiler/project_compiler_v2.h>
#include <apgcore/host/json_contract_v2.h>
#include <apgcore/metadata/atom_catalog.h>
#include <apgcore/registry/registry_builder_v2.h>
#include <apgcore/validator/project_v2.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int usage(const char *argv0) {
    fprintf(
        stderr,
        "usage:\n"
        "  %s validate unit <path>\n"
        "  %s validate project <path>\n"
        "  %s inspect atoms\n"
        "  %s inspect unit <path>\n"
        "  %s inspect project <path>\n"
        "  %s render project <path>\n"
        "  %s benchmark project <path>\n"
        "  %s export --target wasm_realtime [--block-frames <frames>] [--sample-rate <hz>] <project> <outdir>\n"
        "  %s export --target m7_static <project> <outdir>\n"
        "  %s export --target m7_static [--max-static-ram <bytes>] [--block-frames <frames>] [--sample-rate <hz>] "
        "[--cache-line-bytes <bytes>] <project> <outdir>\n",
        argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0
    );
    return 2;
}

static void write_json_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text) {
        for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
            if (*p == '"' || *p == '\\')
                fputc('\\', out);
            if (*p == '\n')
                fputs("\\n", out);
            else if (*p >= 0x20)
                fputc(*p, out);
        }
    }
    fputc('"', out);
}

static void write_c_string(FILE *out, const char *text) {
    fputc('"', out);
    if (text) {
        for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
            if (*p == '"' || *p == '\\')
                fputc('\\', out);
            if (*p == '\n')
                fputs("\\n", out);
            else if (*p >= 0x20 && *p < 0x7f)
                fputc(*p, out);
            else
                fprintf(out, "\\x%02x", (unsigned)*p);
        }
    }
    fputc('"', out);
}

static const char *status_code(uc_status status) {
    switch (status) {
    case UC_OK:
        return "APG_OK";
    case UC_E_IO:
        return "APG_IO_ERROR";
    case UC_E_OOM:
        return "APG_OUT_OF_MEMORY";
    case UC_E_LEX:
        return "APG_LEX_ERROR";
    case UC_E_PARSE:
        return "APG_PARSE_ERROR";
    case UC_E_TYPE:
        return "APG_TYPE_ERROR";
    case UC_E_RANGE:
        return "APG_RANGE_ERROR";
    case UC_E_MISSING:
        return "APG_MISSING_ERROR";
    }
    return "APG_ERROR";
}

static int write_cli_error(FILE *out, const char *schema, const char *file, const char *target, const uc_error *err) {
    fputs("{\"schema\":", out);
    write_json_string(out, schema);
    fputs(",\"ok\":false,\"file\":", out);
    write_json_string(out, file);
    if (target) {
        fputs(",\"target\":", out);
        write_json_string(out, target);
    }
    fputs(",\"diagnostics\":[{\"code\":", out);
    write_json_string(out, status_code(err ? err->status : UC_E_TYPE));
    fputs(",\"message\":", out);
    write_json_string(out, err && err->msg[0] ? err->msg : "operation failed");
    fputs("}]}\n", out);
    return 1;
}

static uc_status load_compile_project(
    const char                *path,
    uc_arena                  *arena,
    apg_project_v2_resolved_t *project,
    apg_project_v2_compiled_t *compiled,
    uc_error                  *err
) {
    uc_status status = apg_project_v2_load_resolved_file(path, arena, project, err);
    if (status == UC_OK)
        status = apg_project_v2_compile(project, arena, compiled, err);
    return status;
}

static int benchmark_project(const char *path) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.benchmark.v2", path, NULL, &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.benchmark.v2", path, NULL, &err);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.benchmark.v2\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, path);
    fprintf(
        stdout,
        ",\"sample_rate\":48000,\"block_frames\":64,\"iterations\":0,"
        "\"structural\":{\"units\":%zu,\"project_nodes\":%zu,\"params\":%zu,\"signals\":%zu,\"nodes\":%zu,"
        "\"schedule\":%zu},\"timing\":{\"available\":false}}\n",
        project.units_len, project.project.nodes_len, compiled.expanded_unit.params_len,
        compiled.expanded_unit.signals_len, compiled.plan.nodes_len, compiled.plan.schedule_len
    );
    uc_arena_free(&arena);
    return 0;
}

typedef enum {
    APG_TARGET_SUPPORT_SUPPORTED,
    APG_TARGET_SUPPORT_UNSUPPORTED,
    APG_TARGET_SUPPORT_UNDECLARED,
} apg_target_support_t;

static apg_target_support_t unit_profile_support(const apg_unit_v2_t *unit, const char *target) {
    for (size_t i = 0; unit && i < unit->compatibility_len; i++) {
        if (!unit->compatibility[i].target || strcmp(unit->compatibility[i].target, target) != 0)
            continue;
        if (unit->compatibility[i].supported && strcmp(unit->compatibility[i].supported, "true") == 0)
            return APG_TARGET_SUPPORT_SUPPORTED;
        return APG_TARGET_SUPPORT_UNSUPPORTED;
    }
    return APG_TARGET_SUPPORT_UNDECLARED;
}

static const apg_project_v2_loaded_unit_t *
first_unsupported_unit(const apg_project_v2_resolved_t *project, const char *target, const char **reason) {
    for (size_t i = 0; i < project->units_len; i++) {
        apg_target_support_t support = unit_profile_support(&project->units[i].unit, target);
        if (support == APG_TARGET_SUPPORT_SUPPORTED)
            continue;
        if (reason) {
            if (support == APG_TARGET_SUPPORT_UNSUPPORTED) {
                if (strcmp(target, "wasm_realtime") == 0)
                    *reason = "does not support wasm_realtime";
                else if (strcmp(target, "m7_static") == 0)
                    *reason = "does not support m7_static";
                else
                    *reason = "does not support target profile";
            } else {
                if (strcmp(target, "m7_static") == 0)
                    *reason = "does not declare m7_static compatibility";
                else if (strcmp(target, "wasm_realtime") == 0)
                    *reason = "does not declare wasm_realtime compatibility";
                else
                    *reason = "does not declare target profile compatibility";
            }
        }
        return &project->units[i];
    }
    return NULL;
}

static const char *first_unsupported_atom(const apg_v2_compiled_unit_t *plan, const char *target) {
    for (size_t i = 0; plan && i < plan->nodes_len; i++) {
        const char *atom_name = plan->nodes[i].atom_name;
        if (!apg_atom_profile_supported(atom_name, target))
            return atom_name;
    }
    return NULL;
}

static bool join_path(char *out, size_t out_size, const char *dir, const char *name) {
    size_t dir_len = dir ? strlen(dir) : 0u;
    int    written =
        snprintf(out, out_size, "%s%s%s", dir ? dir : "", dir_len > 0u && dir[dir_len - 1u] == '/' ? "" : "/", name);
    return written >= 0 && (size_t)written < out_size;
}

static bool validate_export_output_dir(const char *out_dir, uc_error *err) {
    struct stat info;
    if (!out_dir || out_dir[0] == '\0' || stat(out_dir, &info) != 0) {
        uc_error_set(
            err, UC_E_IO, (uc_loc){0, 0}, "export output directory does not exist: '%s'", out_dir ? out_dir : ""
        );
        return false;
    }
    if (!S_ISDIR(info.st_mode)) {
        uc_error_set(err, UC_E_IO, (uc_loc){0, 0}, "export output path is not a directory: '%s'", out_dir);
        return false;
    }
    return true;
}

enum {
    APG_M7_DEFAULT_BLOCK_FRAMES = 64u,
    APG_M7_DEFAULT_SAMPLE_RATE  = 48000u,
    APG_M7_DEFAULT_CACHE_LINE   = 32u,
};

enum {
    APG_WASM_DEFAULT_BLOCK_FRAMES = 64u,
    APG_WASM_DEFAULT_SAMPLE_RATE  = 48000u,
};

typedef struct {
    uint32_t block_frames;
    uint32_t sample_rate;
    uint32_t cache_line_bytes;
    size_t   static_ram_budget;
    bool     has_static_ram_budget;
} m7_export_options_t;

typedef struct {
    size_t signal_buffer_bytes;
    size_t param_bytes;
    size_t schedule_bytes;
    size_t atom_call_bytes;
    size_t signal_array_pointer_count;
    size_t signal_array_pointer_bytes;
    size_t atom_storage_bytes;
    size_t state_buffer_bytes;
    size_t static_ram_bytes;
} m7_memory_manifest_t;

typedef struct {
    uint32_t block_frames;
    uint32_t sample_rate;
} wasm_export_options_t;

static bool parse_size_arg(const char *text, size_t *out) {
    if (!text || !out || text[0] == '\0')
        return false;
    char              *end   = NULL;
    unsigned long long value = strtoull(text, &end, 10);
    if (!end || *end != '\0' || value > (unsigned long long)SIZE_MAX)
        return false;
    *out = (size_t)value;
    return true;
}

static bool parse_uint32_arg(const char *text, uint32_t *out) {
    size_t value = 0u;
    if (!parse_size_arg(text, &value) || value == 0u || value > UINT32_MAX)
        return false;
    *out = (uint32_t)value;
    return true;
}

static bool write_wasm_runtime_js(
    const char                      *path,
    const apg_project_v2_resolved_t *project,
    const apg_v2_registry_t         *registry,
    const wasm_export_options_t     *options,
    bool                             wasm_module_available
) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("export const profile = \"wasm_realtime\";\n", out);
    fputs("export const command = \"audio-worklet-contract\";\n", out);
    fputs(
        "export {createRuntime, runtimeMethods, workletProcessorName} from \"./apg_project_wasm_adapter.mjs\";\n", out
    );
    if (project && project->project.name)
        fprintf(out, "export const projectName = \"%s\";\n", project->project.name);
    else
        fputs("export const projectName = \"unknown\";\n", out);
    fputs("export const runtime = {\n", out);
    fprintf(out, "  blockFrames: %uu,\n", options ? options->block_frames : APG_WASM_DEFAULT_BLOCK_FRAMES);
    fprintf(out, "  sampleRate: %uu,\n", options ? options->sample_rate : APG_WASM_DEFAULT_SAMPLE_RATE);
    if (project && project->project.name)
        fprintf(out, "  project: \"%s\",\n", project->project.name);
    else
        fputs("  project: \"unknown\",\n", out);
    if (project)
        fprintf(out, "  units: %zu,\n", project->units_len);
    if (registry)
        fprintf(out, "  nodes: %zu,\n  schedule: %zu,\n", registry->nodes_len, registry->schedule_len);
    fputs("  wasmModule: \"apg_project_wasm.wasm\",\n", out);
    fprintf(out, "  wasmModuleAvailable: %s,\n", wasm_module_available ? "true" : "false");
    fputs("};\n", out);
    return fclose(out) == 0;
}

static void write_wasm_string_array(FILE *out, const char *const *names, size_t names_len) {
    fputc('[', out);
    for (size_t i = 0; i < names_len; i++) {
        if (i > 0u)
            fputc(',', out);
        write_c_string(out, names[i] ? names[i] : "");
    }
    fputc(']', out);
}

static void write_wasm_bypass_names(FILE *out, const apg_v2_registry_t *registry) {
    fputc('[', out);
    for (size_t i = 0; registry && i < registry->bypassed_instances_len; i++) {
        if (i > 0u)
            fputc(',', out);
        write_c_string(out, registry->bypass_instances[i].instance_id ? registry->bypass_instances[i].instance_id : "");
    }
    fputc(']', out);
}

static void write_wasm_meter_ports(FILE *out, const apg_v2_registry_audio_port_t *ports, size_t ports_len) {
    fputc('[', out);
    for (size_t i = 0; i < ports_len; i++) {
        if (i > 0u)
            fputc(',', out);
        fputs("{\"name\":", out);
        write_c_string(out, ports[i].port_name ? ports[i].port_name : "");
        fprintf(out, ",\"channels\":%zu,\"meter_index\":%zu}", ports[i].channel_count, ports[i].meter_index);
    }
    fputc(']', out);
}

static bool write_wasm_runtime_adapter_js(const char *path, const apg_v2_registry_t *registry) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("export const workletProcessorName = \"apg-project-wasm-processor\";\n", out);
    fputs(
        "export const runtimeMethods = [\"compile\", \"start\", \"stop\", \"setParam\", \"setBypass\", "
        "\"pollMeters\", \"getLastError\"];\n",
        out
    );
    fputs("export const runtimeNames = {\n  params: ", out);
    write_wasm_string_array(
        out, registry ? (const char *const *)registry->param_names : NULL, registry ? registry->params_len : 0u
    );
    fputs(",\n  bypassInstances: ", out);
    write_wasm_bypass_names(out, registry);
    fputs(",\n  inputMeters: ", out);
    write_wasm_meter_ports(
        out, registry ? registry->input_audio_ports : NULL, registry ? registry->input_audio_ports_len : 0u
    );
    fputs(",\n  outputMeters: ", out);
    write_wasm_meter_ports(
        out, registry ? registry->output_audio_ports : NULL, registry ? registry->output_audio_ports_len : 0u
    );
    fputs("\n};\n", out);
    fputs(
        "const defaultProcessorUrl = new URL(\"./apg_project_wasm_processor.js\", import.meta.url).href;\n"
        "const defaultWasmUrl = new URL(\"./apg_project_wasm.wasm\", import.meta.url).href;\n"
        "function hasName(items, name) {\n"
        "  return items.includes(name);\n"
        "}\n"
        "function emptyMeterSnapshot(port) {\n"
        "  return {name: port.name, peak: Array(port.channels).fill(0), rms: Array(port.channels).fill(0)};\n"
        "}\n"
        "export function createRuntime() {\n"
        "  let lastError = null;\n"
        "  let audioContext = null;\n"
        "  let node = null;\n"
        "  let compiled = false;\n"
        "  const paramValues = new Map(runtimeNames.params.map((name) => [name, 0]));\n"
        "  const bypassStates = new Map(runtimeNames.bypassInstances.map((name) => [name, false]));\n"
        "  function post(type, payload = {}) {\n"
        "    if (node) {\n"
        "      node.port.postMessage({type, ...payload});\n"
        "    }\n"
        "  }\n"
        "  return {\n"
        "    async compile(options = {}) {\n"
        "      const AudioContextCtor = globalThis.AudioContext || globalThis.webkitAudioContext;\n"
        "      audioContext = options.audioContext || audioContext || (AudioContextCtor ? new AudioContextCtor() : "
        "null);\n"
        "      if (!audioContext || !audioContext.audioWorklet) {\n"
        "        lastError = \"AudioWorklet is not available\";\n"
        "        throw new Error(lastError);\n"
        "      }\n"
        "      await audioContext.audioWorklet.addModule(options.processorUrl || defaultProcessorUrl);\n"
        "      compiled = true;\n"
        "      lastError = null;\n"
        "      return {ok: true, runtime: \"audio_worklet_contract\", processor: workletProcessorName};\n"
        "    },\n"
        "    async start(options = {}) {\n"
        "      if (!compiled) {\n"
        "        await this.compile(options);\n"
        "      }\n"
        "      if (!audioContext) {\n"
        "        lastError = \"AudioContext is not initialized\";\n"
        "        throw new Error(lastError);\n"
        "      }\n"
        "      const AudioWorkletNodeCtor = globalThis.AudioWorkletNode;\n"
        "      if (!AudioWorkletNodeCtor) {\n"
        "        lastError = \"AudioWorkletNode is not available\";\n"
        "        throw new Error(lastError);\n"
        "      }\n"
        "      const nodeOptions = Object.assign({}, options.nodeOptions || {});\n"
        "      nodeOptions.processorOptions = Object.assign(\n"
        "        {wasmUrl: options.wasmUrl || defaultWasmUrl},\n"
        "        nodeOptions.processorOptions || {}\n"
        "      );\n"
        "      node = new AudioWorkletNodeCtor(audioContext, workletProcessorName, nodeOptions);\n"
        "      node.port.onmessage = (event) => {\n"
        "        if (event.data && event.data.type === \"error\") {\n"
        "          lastError = event.data.message || \"AudioWorklet runtime error\";\n"
        "        }\n"
        "      };\n"
        "      if (options.input) {\n"
        "        options.input.connect(node);\n"
        "      }\n"
        "      node.connect(options.output || audioContext.destination);\n"
        "      if (audioContext.state === \"suspended\") {\n"
        "        await audioContext.resume();\n"
        "      }\n"
        "      lastError = null;\n"
        "      return {ok: true, node, audioContext};\n"
        "    },\n"
        "    stop() {\n"
        "      if (node) {\n"
        "        node.disconnect();\n"
        "        node = null;\n"
        "      }\n"
        "      lastError = null;\n"
        "      return Promise.resolve({ok: true});\n"
        "    },\n"
        "    setParam(name, value) {\n"
        "      if (!hasName(runtimeNames.params, name)) {\n"
        "        lastError = `unknown param '${name}'`;\n"
        "        return Promise.reject(new Error(lastError));\n"
        "      }\n"
        "      paramValues.set(name, Number(value));\n"
        "      post(\"setParam\", {index: runtimeNames.params.indexOf(name), value: paramValues.get(name)});\n"
        "      lastError = null;\n"
        "      return Promise.resolve({ok: true, name, value: paramValues.get(name)});\n"
        "    },\n"
        "    setBypass(instanceId, bypassed) {\n"
        "      if (!hasName(runtimeNames.bypassInstances, instanceId)) {\n"
        "        lastError = `unknown bypass instance '${instanceId}'`;\n"
        "        return Promise.reject(new Error(lastError));\n"
        "      }\n"
        "      bypassStates.set(instanceId, Boolean(bypassed));\n"
        "      post(\"setBypass\", {index: runtimeNames.bypassInstances.indexOf(instanceId), bypassed: "
        "bypassStates.get(instanceId)});\n"
        "      lastError = null;\n"
        "      return Promise.resolve({ok: true, instanceId, bypassed: bypassStates.get(instanceId)});\n"
        "    },\n"
        "    pollMeters() {\n"
        "      lastError = null;\n"
        "      return Promise.resolve({\n"
        "        inputs: runtimeNames.inputMeters.map(emptyMeterSnapshot),\n"
        "        outputs: runtimeNames.outputMeters.map(emptyMeterSnapshot),\n"
        "        peak: [],\n"
        "        rms: []\n"
        "      });\n"
        "    },\n"
        "    getLastError() {\n"
        "      return lastError;\n"
        "    }\n"
        "  };\n"
        "}\n",
        out
    );
    return fclose(out) == 0;
}

static bool write_wasm_runtime_processor_js(const char *path) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs(
        "class ApgProjectWasmProcessor extends AudioWorkletProcessor {\n"
        "  constructor(options = {}) {\n"
        "    super();\n"
        "    this.ready = false;\n"
        "    this.exports = null;\n"
        "    this.memoryF32 = null;\n"
        "    this.init(options.processorOptions || {});\n"
        "    this.port.onmessage = (event) => this.handleMessage(event.data || {});\n"
        "  }\n"
        "  async init(options) {\n"
        "    try {\n"
        "      const url = options.wasmUrl || \"apg_project_wasm.wasm\";\n"
        "      const response = await fetch(url);\n"
        "      const module = await WebAssembly.instantiate(await response.arrayBuffer(), {});\n"
        "      this.exports = module.instance.exports;\n"
        "      this.memoryF32 = new Float32Array(this.exports.memory.buffer);\n"
        "      if (this.exports.apg_wasm_project_init) {\n"
        "        this.exports.apg_wasm_project_init();\n"
        "      }\n"
        "      this.ready = true;\n"
        "    } catch (error) {\n"
        "      this.port.postMessage({type: \"error\", message: error && error.message ? error.message : "
        "String(error)});\n"
        "    }\n"
        "  }\n"
        "  handleMessage(message) {\n"
        "    if (!this.exports) return;\n"
        "    if (message.type === \"setParam\" && this.exports.apg_wasm_project_set_param) {\n"
        "      this.exports.apg_wasm_project_set_param(message.index >>> 0, Number(message.value));\n"
        "    }\n"
        "  }\n"
        "  copyInput(inputs) {\n"
        "    const input = inputs[0] || [];\n"
        "    const frames = input[0] ? input[0].length : 0;\n"
        "    for (let channel = 0; channel < input.length; channel += 1) {\n"
        "      const ptr = this.exports.apg_wasm_project_input_ptr(0, channel) >>> 2;\n"
        "      this.memoryF32.set(input[channel], ptr);\n"
        "    }\n"
        "    return frames;\n"
        "  }\n"
        "  copyOutput(outputs) {\n"
        "    const output = outputs[0] || [];\n"
        "    for (let channel = 0; channel < output.length; channel += 1) {\n"
        "      const ptr = this.exports.apg_wasm_project_output_ptr(0, channel) >>> 2;\n"
        "      output[channel].set(this.memoryF32.subarray(ptr, ptr + output[channel].length));\n"
        "    }\n"
        "  }\n"
        "  process(inputs, outputs) {\n"
        "    if (this.ready && this.exports && this.memoryF32) {\n"
        "      this.copyInput(inputs);\n"
        "      this.exports.apg_wasm_project_process_block();\n"
        "      this.copyOutput(outputs);\n"
        "      return true;\n"
        "    }\n"
        "    const input = inputs[0] || [];\n"
        "    const output = outputs[0] || [];\n"
        "    const channels = Math.min(input.length, output.length);\n"
        "    for (let channel = 0; channel < channels; channel += 1) {\n"
        "      output[channel].set(input[channel]);\n"
        "    }\n"
        "    return true;\n"
        "  }\n"
        "}\n"
        "registerProcessor(\"apg-project-wasm-processor\", ApgProjectWasmProcessor);\n",
        out
    );
    return fclose(out) == 0;
}

static bool write_wasm_runtime_manifest(
    const char                      *path,
    const apg_project_v2_resolved_t *project,
    const apg_v2_registry_t         *registry,
    const wasm_export_options_t     *options,
    bool                             wasm_module_available
) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("{\"schema\":\"apg.project.wasm_realtime.v2\",\"project\":", out);
    write_json_string(out, project && project->project.name ? project->project.name : "unknown");
    fprintf(
        out,
        ",\"sample_rate\":%u,\"block_frames\":%u,\"runtime\":\"audio_worklet_wasm\","
        "\"layout\":{\"params\":%zu,\"signals\":%zu,\"nodes\":%zu,\"schedule\":%zu},\"status\":\"generated\"",
        options ? options->sample_rate : APG_WASM_DEFAULT_SAMPLE_RATE,
        options ? options->block_frames : APG_WASM_DEFAULT_BLOCK_FRAMES, registry ? registry->params_len : 0u,
        registry ? registry->signals_len : 0u, registry ? registry->nodes_len : 0u,
        registry ? registry->schedule_len : 0u
    );
    fputs(
        ",\"artifacts\":{\"manifest\":\"apg_project_wasm.json\",\"entry_js\":\"apg_project_wasm.mjs\","
        "\"adapter_js\":\"apg_project_wasm_adapter.mjs\","
        "\"worklet_processor_js\":\"apg_project_wasm_processor.js\","
        "\"wasm_header\":\"apg_project_wasm.h\",\"wasm_source\":\"apg_project_wasm.c\","
        "\"wasm_module\":\"apg_project_wasm.wasm\",\"wasm_module_available\":",
        out
    );
    fputs(wasm_module_available ? "true" : "false", out);
    fputc('}', out);
    fputs(
        ",\"files\":[\"apg_project_wasm.json\",\"apg_project_wasm.mjs\","
        "\"apg_project_wasm_adapter.mjs\",\"apg_project_wasm_processor.js\","
        "\"apg_project_wasm.h\",\"apg_project_wasm.c\"",
        out
    );
    if (wasm_module_available)
        fputs(",\"apg_project_wasm.wasm\"", out);
    fputs("]}\n", out);
    return fclose(out) == 0;
}

static bool parse_alignment_arg(const char *text, uint32_t *out) {
    uint32_t value = 0u;
    if (!parse_uint32_arg(text, &value) || (value & (value - 1u)) != 0u)
        return false;
    *out = value;
    return true;
}

static m7_memory_manifest_t m7_memory_manifest(const apg_v2_registry_t *registry) {
    m7_memory_manifest_t memory = {0};
    if (!registry)
        return memory;

    memory.signal_buffer_bytes        = registry->signal_samples * sizeof(float);
    memory.param_bytes                = registry->params_len * sizeof(float);
    memory.schedule_bytes             = registry->schedule_len * sizeof(uint32_t);
    memory.atom_call_bytes            = registry->nodes_len * sizeof(atom_call_t);
    memory.signal_array_pointer_count = registry->signal_array_pointer_slots;
    memory.signal_array_pointer_bytes = registry->signal_array_pointer_slots * sizeof(float *);
    memory.atom_storage_bytes         = registry->atom_storage_bytes;
    memory.state_buffer_bytes         = registry->state_buffer_samples * sizeof(float);
    memory.static_ram_bytes           = memory.signal_buffer_bytes + memory.param_bytes + memory.atom_call_bytes +
                              memory.signal_array_pointer_bytes + memory.atom_storage_bytes + memory.state_buffer_bytes;
    return memory;
}

static bool write_m7_header(
    const char                 *path,
    const apg_v2_registry_t    *registry,
    const m7_memory_manifest_t *memory,
    const m7_export_options_t  *options
) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#ifndef APG_PROJECT_M7_BUNDLE_H\n#define APG_PROJECT_M7_BUNDLE_H\n\n", out);
    fputs("#include <stddef.h>\n#include <stdint.h>\n#include <atom_registry.h>\n\n", out);
    fprintf(out, "#define APG_M7_PROJECT_PARAM_COUNT %zuu\n", registry->params_len);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_COUNT %zuu\n", registry->signals_len);
    fprintf(out, "#define APG_M7_PROJECT_NODE_COUNT %zuu\n", registry->nodes_len);
    fprintf(out, "#define APG_M7_PROJECT_SCHEDULE_COUNT %zuu\n", registry->schedule_len);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_COUNT %zuu\n\n", memory->signal_array_pointer_count);
    fprintf(out, "#define APG_M7_PROJECT_BLOCK_FRAMES %uu\n", registry->frame_capacity);
    fprintf(out, "#define APG_M7_PROJECT_SAMPLE_RATE %uu\n", (unsigned)registry->sample_rate);
    fprintf(out, "#define APG_M7_PROJECT_ATOM_CALLS_PER_BLOCK %zuu\n", registry->schedule_len);
    fprintf(
        out, "#define APG_M7_PROJECT_ATOM_CALLS_PER_SECOND %zuu\n",
        registry->frame_capacity ? registry->schedule_len * (size_t)registry->sample_rate / registry->frame_capacity
                                 : 0u
    );
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_BUFFER_BYTES %zuu\n", memory->signal_buffer_bytes);
    fprintf(out, "#define APG_M7_PROJECT_PARAM_BYTES %zuu\n", memory->param_bytes);
    fprintf(out, "#define APG_M7_PROJECT_SCHEDULE_BYTES %zuu\n", memory->schedule_bytes);
    fprintf(out, "#define APG_M7_PROJECT_ATOM_CALL_BYTES %zuu\n", memory->atom_call_bytes);
    fprintf(out, "#define APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_BYTES %zuu\n", memory->signal_array_pointer_bytes);
    fprintf(out, "#define APG_M7_PROJECT_ATOM_STORAGE_BYTES %zuu\n", memory->atom_storage_bytes);
    fprintf(out, "#define APG_M7_PROJECT_STATE_BUFFER_BYTES %zuu\n", memory->state_buffer_bytes);
    fprintf(out, "#define APG_M7_PROJECT_STATIC_RAM_BYTES %zuu\n\n", memory->static_ram_bytes);
    fprintf(out, "#define APG_M7_CACHE_LINE_BYTES %uu\n\n", options->cache_line_bytes);
    fputs("#define APG_M7_SECTION_SIGNAL_BUFFERS \".apg_m7_signal_buffers\"\n", out);
    fputs("#define APG_M7_SECTION_PARAMS \".apg_m7_params\"\n", out);
    fputs("#define APG_M7_SECTION_ATOM_CALLS \".apg_m7_atom_calls\"\n", out);
    fputs("#define APG_M7_SECTION_SIGNAL_ARRAYS \".apg_m7_signal_arrays\"\n", out);
    fputs("#define APG_M7_SECTION_ATOM_STORAGE \".apg_m7_atom_storage\"\n", out);
    fputs("#define APG_M7_SECTION_STATE_BUFFERS \".apg_m7_state_buffers\"\n", out);
    fputs("#if defined(__GNUC__)\n", out);
    fputs("#define APG_M7_SECTION_ATTR(name) __attribute__((section(name), aligned(APG_M7_CACHE_LINE_BYTES)))\n", out);
    fputs("#else\n#define APG_M7_SECTION_ATTR(name)\n#endif\n\n", out);
    fputs("#define APG_M7_PROJECT_USES_RUNTIME_YAML 0u\n", out);
    fputs("#define APG_M7_PROJECT_USES_DYNAMIC_ALLOCATION 0u\n\n", out);
    fputs("extern const char apg_m7_project_name[];\n", out);
    fputs("extern const uint32_t apg_m7_project_schedule[APG_M7_PROJECT_SCHEDULE_COUNT];\n", out);
    fputs("extern const char *const apg_m7_project_nodes[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("extern const char *const apg_m7_project_atom_process_symbols[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("extern const atom_thunk_fn apg_m7_project_atom_thunks[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("extern const apg_process_info_t apg_m7_project_process_info;\n", out);
    fputs("extern atom_call_t apg_m7_project_atom_calls[APG_M7_PROJECT_NODE_COUNT];\n", out);
    fputs("void apg_m7_project_init(void);\n", out);
    fputs("void apg_m7_project_refresh_params(void);\n", out);
    fputs("void apg_m7_project_process_block(void);\n", out);
    fputs("\n#endif\n", out);
    return fclose(out) == 0;
}

static void write_c_float(FILE *out, float value) {
    char text[48];
    snprintf(text, sizeof(text), "%.9g", (double)value);
    fputs(text, out);
    if (!strchr(text, '.') && !strchr(text, 'e') && !strchr(text, 'E'))
        fputs(".0", out);
    fputc('f', out);
}

static void write_m7_scalar_value(FILE *out, const apg_v2_registry_scalar_refresh_t *item) {
    if (!item) {
        fputs("0.0f", out);
    } else if (item->kind == APG_BIND_PARAM) {
        fprintf(out, "apg_m7_param(%zuu)", item->param_index);
    } else if (item->kind == APG_BIND_LITERAL) {
        write_c_float(out, item->number);
    } else {
        fputs("0.0f", out);
    }
}

static void write_m7_node_storage_ptr(FILE *out, const char *atom_name, const char *suffix, size_t offset) {
    fprintf(
        out, "((%s_%s_t *)(void *)&apg_m7_project_atom_storage[%zuu])", atom_name ? atom_name : "void",
        suffix ? suffix : "out", offset
    );
}

static bool
write_m7_source(const char *path, const apg_project_v2_resolved_t *project, const apg_v2_registry_t *registry) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#include \"apg_project_m7.h\"\n\n", out);
    fputs("#include <string.h>\n\n", out);
    fputs("#include <atom/dsp_types.h>\n\n", out);
    fputs("const char apg_m7_project_name[] = ", out);
    write_c_string(out, project->project.name);
    fputs(";\n\nconst uint32_t apg_m7_project_schedule[APG_M7_PROJECT_SCHEDULE_COUNT] = {", out);
    for (size_t i = 0; i < registry->schedule_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fprintf(out, "%uu", (unsigned)registry->schedule[i]);
    }
    fputs("};\n\nconst char *const apg_m7_project_nodes[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        write_c_string(out, registry->node_layouts[i].node_id);
    }
    fputs("};\n\n", out);
    fputs("const apg_process_info_t apg_m7_project_process_info = {", out);
    fprintf(
        out, ".sample_rate = %.1ff, .frames = %uu, .output_frames = %uu, .channels = 1u", (double)registry->sample_rate,
        registry->frame_capacity, registry->frame_capacity
    );
    fputs("};\n\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        bool seen = false;
        // ponytail: O(n^2) is fine for generated project node counts; sort/dedupe if exports grow large.
        for (size_t j = 0; j < i; j++) {
            if (strcmp(registry->node_layouts[j].atom_name, registry->node_layouts[i].atom_name) == 0) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;
        fputs("extern void ", out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_thunk(atom_call_t *call);\n", out);
    }
    fputs("\nconst atom_thunk_fn apg_m7_project_atom_thunks[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_thunk", out);
    }
    fputs("};\n\n", out);
    fputs("const char *const apg_m7_project_atom_process_symbols[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fputc('"', out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_process\"", out);
    }
    fputs("};\n\n", out);
    fputs("#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_signal_buffers[APG_M7_PROJECT_SIGNAL_BUFFER_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_SIGNAL_BUFFERS);\n",
        out
    );
    fputs("#endif\n\n#if APG_M7_PROJECT_PARAM_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_params[APG_M7_PROJECT_PARAM_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_PARAMS);\n",
        out
    );
    fputs("#endif\n\n#if APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_BYTES > 0u\n", out);
    fputs(
        "float *apg_m7_project_signal_array_pool[APG_M7_PROJECT_SIGNAL_ARRAY_POINTER_COUNT] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_SIGNAL_ARRAYS);\n",
        out
    );
    fputs("#endif\n\n#if APG_M7_PROJECT_STATE_BUFFER_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_state_buffers[APG_M7_PROJECT_STATE_BUFFER_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_STATE_BUFFERS);\n",
        out
    );
    fputs("#endif\n\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        if (layout->mix_matrix_coefficients_len == 0u)
            continue;
        fprintf(out, "static float apg_m7_node%zu_mix_coefficients[%zu] = {", i, layout->mix_matrix_coefficients_len);
        for (size_t j = 0; j < layout->mix_matrix_coefficients_len; j++) {
            if (j > 0u)
                fputs(", ", out);
            write_c_float(out, layout->mix_matrix_coefficients[j]);
        }
        fputs("};\n", out);
        fprintf(out, "static float *apg_m7_node%zu_mix_rows[%zu] = {", i, layout->mix_matrix_num_out);
        for (size_t row = 0; row < layout->mix_matrix_num_out; row++) {
            if (row > 0u)
                fputs(", ", out);
            fprintf(out, "&apg_m7_node%zu_mix_coefficients[%zuu]", i, row * layout->mix_matrix_num_in);
        }
        fputs("};\n\n", out);
    }
    fputs("#if APG_M7_PROJECT_ATOM_STORAGE_BYTES > 0u\n", out);
    fputs(
        "uint8_t apg_m7_project_atom_storage[APG_M7_PROJECT_ATOM_STORAGE_BYTES] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_ATOM_STORAGE);\n",
        out
    );
    fputs("#endif\n\n", out);
    fputs("static const apg_spectral_info_t apg_m7_project_spectral_info[APG_M7_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        const apg_spectral_info_t *info = &registry->node_layouts[i].spectral_info;
        fprintf(
            out, "{.fft_size = %uu, .bin_count = %uu, .hop_size = %uu}", info->fft_size, info->bin_count, info->hop_size
        );
    }
    fputs("};\n\n", out);
    fputs(
        "atom_call_t apg_m7_project_atom_calls[APG_M7_PROJECT_NODE_COUNT] "
        "APG_M7_SECTION_ATTR(APG_M7_SECTION_ATOM_CALLS) = {",
        out
    );
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        fprintf(
            out,
            "{.out = (void *)&apg_m7_project_atom_storage[%zuu], .in = (void *)&apg_m7_project_atom_storage[%zuu], "
            ".config = (void *)&apg_m7_project_atom_storage[%zuu], "
            ".state = (void *)&apg_m7_project_atom_storage[%zuu], .info = &apg_m7_project_process_info, "
            ".spectral_info = ",
            layout->out_offset, layout->in_offset, layout->config_offset, layout->state_offset
        );
        if (layout->has_spectral_info)
            fprintf(out, "&apg_m7_project_spectral_info[%zuu]", i);
        else
            fputs("NULL", out);
        fputc('}', out);
    }
    fputs("};\n\n", out);
    fputs("#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u\n", out);
    fputs("static float *apg_m7_signal(size_t index) {\n", out);
    fputs(
        "    return (float *)(void *)&apg_m7_project_signal_buffers[index * APG_M7_PROJECT_BLOCK_FRAMES * "
        "sizeof(float)];\n",
        out
    );
    fputs("}\n#endif\n\n#if APG_M7_PROJECT_PARAM_BYTES > 0u\n", out);
    fputs("static float apg_m7_param(size_t index) {\n", out);
    fputs("    return ((float *)(void *)apg_m7_project_params)[index];\n}\n#endif\n\n", out);
    fputs("void apg_m7_project_refresh_params(void) {\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        for (size_t j = 0; j < layout->config_refreshes_len; j++) {
            const apg_v2_registry_scalar_refresh_t *item = &layout->config_refreshes[j];
            const char                             *key  = item->key;
            if (!key)
                continue;
            fputs("    ", out);
            write_m7_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->%s = ", key);
            write_m7_scalar_value(out, item);
            fputs(";\n", out);
        }
        for (size_t j = 0; j < layout->input_refreshes_len; j++) {
            const apg_v2_registry_scalar_refresh_t *item = &layout->input_refreshes[j];
            const char                             *key  = item->key;
            if (!key)
                continue;
            fputs("    ", out);
            write_m7_node_storage_ptr(out, layout->atom_name, "in", layout->in_offset);
            fprintf(out, "->%s = ", key);
            write_m7_scalar_value(out, item);
            fputs(";\n", out);
        }
    }
    fputs("}\n\nvoid apg_m7_project_init(void) {\n", out);
    fputs("#if APG_M7_PROJECT_ATOM_STORAGE_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_atom_storage, 0, APG_M7_PROJECT_ATOM_STORAGE_BYTES);\n", out);
    fputs("#endif\n", out);
    fputs("#if APG_M7_PROJECT_SIGNAL_BUFFER_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_signal_buffers, 0, APG_M7_PROJECT_SIGNAL_BUFFER_BYTES);\n", out);
    fputs("#endif\n#if APG_M7_PROJECT_PARAM_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_params, 0, APG_M7_PROJECT_PARAM_BYTES);\n", out);
    fputs("#endif\n#if APG_M7_PROJECT_STATE_BUFFER_BYTES > 0u\n", out);
    fputs("    memset(apg_m7_project_state_buffers, 0, APG_M7_PROJECT_STATE_BUFFER_BYTES);\n", out);
    fputs("#endif\n", out);
    for (size_t i = 0; i < registry->params_len; i++) {
        fputs("    ((float *)(void *)apg_m7_project_params)[", out);
        fprintf(out, "%zuu] = ", i);
        write_c_float(out, registry->param_defaults ? registry->param_defaults[i] : 0.0f);
        fputs(";\n", out);
    }
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        for (size_t j = 0; j < layout->signal_bindings_len; j++) {
            const apg_v2_registry_signal_binding_t *binding = &layout->signal_bindings[j];
            const char                             *key     = binding->key;
            if (!key)
                continue;
            if (binding->is_signal_array) {
                for (size_t k = 0; k < binding->signal_array_len; k++) {
                    fprintf(
                        out, "    apg_m7_project_signal_array_pool[%zuu] = apg_m7_signal(%zuu);\n",
                        layout->signal_array_pool_offset + binding->signal_array_offset + k,
                        binding->signal_array_indices[k]
                    );
                }
                fputs("    ", out);
                write_m7_node_storage_ptr(
                    out, layout->atom_name, binding->is_input ? "in" : "out",
                    binding->is_input ? layout->in_offset : layout->out_offset
                );
                fprintf(
                    out, "->%s = &apg_m7_project_signal_array_pool[%zuu];\n", key,
                    layout->signal_array_pool_offset + binding->signal_array_offset
                );
            } else {
                fputs("    ", out);
                write_m7_node_storage_ptr(
                    out, layout->atom_name, binding->is_input ? "in" : "out",
                    binding->is_input ? layout->in_offset : layout->out_offset
                );
                fprintf(out, "->%s = apg_m7_signal(%zuu);\n", key, binding->signal_index);
            }
        }
        size_t state_buffer_index = 0u;
        for (int field_index = 0; field_index < layout->n_state_fields; field_index++) {
            const atom_field_desc_t *field = &layout->state_fields[field_index];
            if (field->type != FIELD_BUFFER)
                continue;
            fputs("    ", out);
            write_m7_node_storage_ptr(out, layout->atom_name, "state", layout->state_offset);
            fprintf(
                out, "->%s = (float *)(void *)&apg_m7_project_state_buffers[%zuu];\n", field->name,
                layout->state_buffer_sample_offsets_by_index[state_buffer_index] * sizeof(float)
            );
            state_buffer_index++;
        }
        if (layout->mix_matrix_coefficients_len > 0u) {
            fputs("    ", out);
            write_m7_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->coefficients = apg_m7_node%zu_mix_rows;\n", i);
            fputs("    ", out);
            write_m7_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->num_in = %zu;\n", layout->mix_matrix_num_in);
            fputs("    ", out);
            write_m7_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->num_out = %zu;\n", layout->mix_matrix_num_out);
        }
    }
    fputs("    apg_m7_project_refresh_params();\n}\n\n", out);
    fputs("void apg_m7_project_process_block(void) {\n", out);
    fputs("    for (size_t i = 0u; i < APG_M7_PROJECT_SCHEDULE_COUNT; i++) {\n", out);
    fputs("        uint32_t node = apg_m7_project_schedule[i];\n", out);
    fputs("        apg_m7_project_atom_thunks[node](&apg_m7_project_atom_calls[node]);\n", out);
    fputs("    }\n}\n", out);
    return fclose(out) == 0;
}

static bool write_wasm_header(const char *path, const apg_v2_registry_t *registry) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#ifndef APG_PROJECT_WASM_BUNDLE_H\n#define APG_PROJECT_WASM_BUNDLE_H\n\n", out);
    fputs("#include <stddef.h>\n#include <stdint.h>\n#include <atom_registry.h>\n\n", out);
    fprintf(out, "#define APG_WASM_PROJECT_PARAM_COUNT %zuu\n", registry->params_len);
    fprintf(out, "#define APG_WASM_PROJECT_SIGNAL_COUNT %zuu\n", registry->signals_len);
    fprintf(out, "#define APG_WASM_PROJECT_NODE_COUNT %zuu\n", registry->nodes_len);
    fprintf(out, "#define APG_WASM_PROJECT_SCHEDULE_COUNT %zuu\n", registry->schedule_len);
    fprintf(out, "#define APG_WASM_PROJECT_SIGNAL_ARRAY_POINTER_COUNT %zuu\n", registry->signal_array_pointer_slots);
    fprintf(out, "#define APG_WASM_PROJECT_BLOCK_FRAMES %uu\n", registry->frame_capacity);
    fprintf(out, "#define APG_WASM_PROJECT_SAMPLE_RATE %uu\n", (unsigned)registry->sample_rate);
    fprintf(out, "#define APG_WASM_PROJECT_SIGNAL_BUFFER_BYTES %zuu\n", registry->signal_samples * sizeof(float));
    fprintf(out, "#define APG_WASM_PROJECT_PARAM_BYTES %zuu\n", registry->params_len * sizeof(float));
    fprintf(out, "#define APG_WASM_PROJECT_ATOM_STORAGE_BYTES %zuu\n", registry->atom_storage_bytes);
    fprintf(
        out, "#define APG_WASM_PROJECT_STATE_BUFFER_BYTES %zuu\n\n", registry->state_buffer_samples * sizeof(float)
    );
    fputs("extern const char apg_wasm_project_name[];\n", out);
    fputs("extern const uint32_t apg_wasm_project_schedule[APG_WASM_PROJECT_SCHEDULE_COUNT];\n", out);
    fputs("extern const apg_process_info_t apg_wasm_project_process_info;\n", out);
    fputs("extern atom_call_t apg_wasm_project_atom_calls[APG_WASM_PROJECT_NODE_COUNT];\n", out);
    fputs("uint32_t apg_wasm_project_block_frames(void);\n", out);
    fputs("uint32_t apg_wasm_project_input_ptr(uint32_t port_index, uint32_t channel_index);\n", out);
    fputs("uint32_t apg_wasm_project_output_ptr(uint32_t port_index, uint32_t channel_index);\n", out);
    fputs("void apg_wasm_project_set_param(uint32_t param_index, float value);\n", out);
    fputs("void apg_wasm_project_init(void);\n", out);
    fputs("void apg_wasm_project_refresh_params(void);\n", out);
    fputs("void apg_wasm_project_process_block(void);\n", out);
    fputs("\n#endif\n", out);
    return fclose(out) == 0;
}

static void write_wasm_node_storage_ptr(FILE *out, const char *atom_name, const char *suffix, size_t offset) {
    fprintf(
        out, "((%s_%s_t *)(void *)&apg_wasm_project_atom_storage[%zuu])", atom_name ? atom_name : "void",
        suffix ? suffix : "out", offset
    );
}

static void write_wasm_scalar_value(FILE *out, const apg_v2_registry_scalar_refresh_t *item) {
    if (!item) {
        fputs("0.0f", out);
    } else if (item->kind == APG_BIND_PARAM) {
        fprintf(out, "apg_wasm_param(%zuu)", item->param_index);
    } else if (item->kind == APG_BIND_LITERAL) {
        write_c_float(out, item->number);
    } else {
        fputs("0.0f", out);
    }
}

static bool
write_wasm_source(const char *path, const apg_project_v2_resolved_t *project, const apg_v2_registry_t *registry) {
    FILE *out = fopen(path, "w");
    if (!out)
        return false;
    fputs("#include \"apg_project_wasm.h\"\n\n", out);
    fputs("#include <stdint.h>\n#include <string.h>\n\n", out);
    fputs("#include <atom/dsp_types.h>\n\n", out);
    fputs("#if defined(__EMSCRIPTEN__)\n", out);
    fputs("#define APG_WASM_EXPORT __attribute__((used, visibility(\"default\")))\n", out);
    fputs("#else\n#define APG_WASM_EXPORT\n#endif\n\n", out);
    fputs("const char apg_wasm_project_name[] = ", out);
    write_c_string(out, project->project.name);
    fputs(";\n\nconst uint32_t apg_wasm_project_schedule[APG_WASM_PROJECT_SCHEDULE_COUNT] = {", out);
    for (size_t i = 0; i < registry->schedule_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fprintf(out, "%uu", (unsigned)registry->schedule[i]);
    }
    fputs("};\n\n", out);
    fputs("const apg_process_info_t apg_wasm_project_process_info = {", out);
    fprintf(
        out, ".sample_rate = %.1ff, .frames = %uu, .output_frames = %uu, .channels = 1u", (double)registry->sample_rate,
        registry->frame_capacity, registry->frame_capacity
    );
    fputs("};\n\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        bool seen = false;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(registry->node_layouts[j].atom_name, registry->node_layouts[i].atom_name) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen)
            fprintf(out, "extern void %s_thunk(atom_call_t *call);\n", registry->node_layouts[i].atom_name);
    }
    fputs("\nstatic const atom_thunk_fn apg_wasm_project_atom_thunks[APG_WASM_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        fputs(registry->node_layouts[i].atom_name, out);
        fputs("_thunk", out);
    }
    fputs("};\n\n", out);
    fputs("static uint8_t apg_wasm_project_signal_buffers[APG_WASM_PROJECT_SIGNAL_BUFFER_BYTES];\n", out);
    fputs("static uint8_t apg_wasm_project_params[APG_WASM_PROJECT_PARAM_BYTES];\n", out);
    fputs("static uint8_t apg_wasm_project_atom_storage[APG_WASM_PROJECT_ATOM_STORAGE_BYTES];\n", out);
    fputs("static uint8_t apg_wasm_project_state_buffers[APG_WASM_PROJECT_STATE_BUFFER_BYTES];\n", out);
    fputs("static float *apg_wasm_project_signal_array_pool[APG_WASM_PROJECT_SIGNAL_ARRAY_POINTER_COUNT];\n\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        if (layout->mix_matrix_coefficients_len == 0u)
            continue;
        fprintf(out, "static float apg_wasm_node%zu_mix_coefficients[%zu] = {", i, layout->mix_matrix_coefficients_len);
        for (size_t j = 0; j < layout->mix_matrix_coefficients_len; j++) {
            if (j > 0u)
                fputs(", ", out);
            write_c_float(out, layout->mix_matrix_coefficients[j]);
        }
        fputs("};\n", out);
        fprintf(out, "static float *apg_wasm_node%zu_mix_rows[%zu] = {", i, layout->mix_matrix_num_out);
        for (size_t row = 0; row < layout->mix_matrix_num_out; row++) {
            if (row > 0u)
                fputs(", ", out);
            fprintf(out, "&apg_wasm_node%zu_mix_coefficients[%zuu]", i, row * layout->mix_matrix_num_in);
        }
        fputs("};\n\n", out);
    }
    fputs("static const apg_spectral_info_t apg_wasm_project_spectral_info[APG_WASM_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        const apg_spectral_info_t *info = &registry->node_layouts[i].spectral_info;
        fprintf(
            out, "{.fft_size = %uu, .bin_count = %uu, .hop_size = %uu}", info->fft_size, info->bin_count, info->hop_size
        );
    }
    fputs("};\n\n", out);
    fputs("atom_call_t apg_wasm_project_atom_calls[APG_WASM_PROJECT_NODE_COUNT] = {", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        if (i > 0u)
            fputs(", ", out);
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        fprintf(
            out,
            "{.out = (void *)&apg_wasm_project_atom_storage[%zuu], .in = (void "
            "*)&apg_wasm_project_atom_storage[%zuu], .config = (void "
            "*)&apg_wasm_project_atom_storage[%zuu], .state = (void "
            "*)&apg_wasm_project_atom_storage[%zuu], .info = &apg_wasm_project_process_info, .spectral_info = ",
            layout->out_offset, layout->in_offset, layout->config_offset, layout->state_offset
        );
        if (layout->has_spectral_info)
            fprintf(out, "&apg_wasm_project_spectral_info[%zuu]", i);
        else
            fputs("NULL", out);
        fputc('}', out);
    }
    fputs("};\n\n", out);
    fputs("static float *apg_wasm_signal(size_t index) {\n", out);
    fputs(
        "    return (float *)(void *)&apg_wasm_project_signal_buffers[index * APG_WASM_PROJECT_BLOCK_FRAMES * "
        "sizeof(float)];\n",
        out
    );
    fputs("}\n\nstatic float apg_wasm_param(size_t index) {\n", out);
    fputs("    return ((float *)(void *)apg_wasm_project_params)[index];\n}\n\n", out);
    fputs(
        "APG_WASM_EXPORT uint32_t apg_wasm_project_block_frames(void) { return APG_WASM_PROJECT_BLOCK_FRAMES; }\n\n",
        out
    );
    fputs("APG_WASM_EXPORT uint32_t apg_wasm_project_input_ptr(uint32_t port_index, uint32_t channel_index) {\n", out);
    fputs("    switch (port_index) {\n", out);
    for (size_t i = 0; i < registry->input_audio_ports_len; i++) {
        fprintf(out, "    case %zuu:\n", i);
        fputs("        switch (channel_index) {\n", out);
        for (size_t ch = 0; ch < registry->input_audio_ports[i].channel_count; ch++)
            fprintf(
                out, "        case %zuu: return (uint32_t)(uintptr_t)apg_wasm_signal(%zuu);\n", ch,
                registry->input_audio_ports[i].signal_indices[ch]
            );
        fputs("        default: return 0u;\n        }\n", out);
    }
    fputs("    default: return 0u;\n    }\n}\n\n", out);
    fputs("APG_WASM_EXPORT uint32_t apg_wasm_project_output_ptr(uint32_t port_index, uint32_t channel_index) {\n", out);
    fputs("    switch (port_index) {\n", out);
    for (size_t i = 0; i < registry->output_audio_ports_len; i++) {
        fprintf(out, "    case %zuu:\n", i);
        fputs("        switch (channel_index) {\n", out);
        for (size_t ch = 0; ch < registry->output_audio_ports[i].channel_count; ch++)
            fprintf(
                out, "        case %zuu: return (uint32_t)(uintptr_t)apg_wasm_signal(%zuu);\n", ch,
                registry->output_audio_ports[i].signal_indices[ch]
            );
        fputs("        default: return 0u;\n        }\n", out);
    }
    fputs("    default: return 0u;\n    }\n}\n\n", out);
    fputs("APG_WASM_EXPORT void apg_wasm_project_set_param(uint32_t param_index, float value) {\n", out);
    fputs("    if (param_index < APG_WASM_PROJECT_PARAM_COUNT) {\n", out);
    fputs("        ((float *)(void *)apg_wasm_project_params)[param_index] = value;\n", out);
    fputs("        apg_wasm_project_refresh_params();\n    }\n}\n\n", out);
    fputs("APG_WASM_EXPORT void apg_wasm_project_refresh_params(void) {\n", out);
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        for (size_t j = 0; j < layout->config_refreshes_len; j++) {
            const apg_v2_registry_scalar_refresh_t *item = &layout->config_refreshes[j];
            if (!item->key)
                continue;
            fputs("    ", out);
            write_wasm_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->%s = ", item->key);
            write_wasm_scalar_value(out, item);
            fputs(";\n", out);
        }
        for (size_t j = 0; j < layout->input_refreshes_len; j++) {
            const apg_v2_registry_scalar_refresh_t *item = &layout->input_refreshes[j];
            if (!item->key)
                continue;
            fputs("    ", out);
            write_wasm_node_storage_ptr(out, layout->atom_name, "in", layout->in_offset);
            fprintf(out, "->%s = ", item->key);
            write_wasm_scalar_value(out, item);
            fputs(";\n", out);
        }
    }
    fputs("}\n\nAPG_WASM_EXPORT void apg_wasm_project_init(void) {\n", out);
    fputs("    memset(apg_wasm_project_atom_storage, 0, APG_WASM_PROJECT_ATOM_STORAGE_BYTES);\n", out);
    fputs("    memset(apg_wasm_project_signal_buffers, 0, APG_WASM_PROJECT_SIGNAL_BUFFER_BYTES);\n", out);
    fputs("    memset(apg_wasm_project_params, 0, APG_WASM_PROJECT_PARAM_BYTES);\n", out);
    fputs("    memset(apg_wasm_project_state_buffers, 0, APG_WASM_PROJECT_STATE_BUFFER_BYTES);\n", out);
    for (size_t i = 0; i < registry->params_len; i++) {
        fprintf(out, "    ((float *)(void *)apg_wasm_project_params)[%zuu] = ", i);
        write_c_float(out, registry->param_defaults ? registry->param_defaults[i] : 0.0f);
        fputs(";\n", out);
    }
    for (size_t i = 0; i < registry->nodes_len; i++) {
        const apg_v2_registry_node_layout_t *layout = &registry->node_layouts[i];
        for (size_t j = 0; j < layout->signal_bindings_len; j++) {
            const apg_v2_registry_signal_binding_t *binding = &layout->signal_bindings[j];
            if (!binding->key)
                continue;
            if (binding->is_signal_array) {
                for (size_t k = 0; k < binding->signal_array_len; k++) {
                    fprintf(
                        out, "    apg_wasm_project_signal_array_pool[%zuu] = apg_wasm_signal(%zuu);\n",
                        layout->signal_array_pool_offset + binding->signal_array_offset + k,
                        binding->signal_array_indices[k]
                    );
                }
                fputs("    ", out);
                write_wasm_node_storage_ptr(
                    out, layout->atom_name, binding->is_input ? "in" : "out",
                    binding->is_input ? layout->in_offset : layout->out_offset
                );
                fprintf(
                    out, "->%s = &apg_wasm_project_signal_array_pool[%zuu];\n", binding->key,
                    layout->signal_array_pool_offset + binding->signal_array_offset
                );
            } else {
                fputs("    ", out);
                write_wasm_node_storage_ptr(
                    out, layout->atom_name, binding->is_input ? "in" : "out",
                    binding->is_input ? layout->in_offset : layout->out_offset
                );
                fprintf(out, "->%s = apg_wasm_signal(%zuu);\n", binding->key, binding->signal_index);
            }
        }
        size_t state_buffer_index = 0u;
        for (int field_index = 0; field_index < layout->n_state_fields; field_index++) {
            const atom_field_desc_t *field = &layout->state_fields[field_index];
            if (field->type != FIELD_BUFFER)
                continue;
            fputs("    ", out);
            write_wasm_node_storage_ptr(out, layout->atom_name, "state", layout->state_offset);
            fprintf(
                out, "->%s = (float *)(void *)&apg_wasm_project_state_buffers[%zuu];\n", field->name,
                layout->state_buffer_sample_offsets_by_index[state_buffer_index] * sizeof(float)
            );
            state_buffer_index++;
        }
        if (layout->mix_matrix_coefficients_len > 0u) {
            fputs("    ", out);
            write_wasm_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->coefficients = apg_wasm_node%zu_mix_rows;\n", i);
            fputs("    ", out);
            write_wasm_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->num_in = %zu;\n", layout->mix_matrix_num_in);
            fputs("    ", out);
            write_wasm_node_storage_ptr(out, layout->atom_name, "params", layout->config_offset);
            fprintf(out, "->num_out = %zu;\n", layout->mix_matrix_num_out);
        }
    }
    fputs("    apg_wasm_project_refresh_params();\n}\n\n", out);
    fputs("APG_WASM_EXPORT void apg_wasm_project_process_block(void) {\n", out);
    fputs("    for (size_t i = 0u; i < APG_WASM_PROJECT_SCHEDULE_COUNT; i++) {\n", out);
    fputs("        uint32_t node = apg_wasm_project_schedule[i];\n", out);
    fputs("        apg_wasm_project_atom_thunks[node](&apg_wasm_project_atom_calls[node]);\n", out);
    fputs("    }\n}\n", out);
    return fclose(out) == 0;
}

static bool append_cmd(char *out, size_t out_size, const char *text) {
    size_t used = strlen(out);
    if (used >= out_size)
        return false;
    int written = snprintf(out + used, out_size - used, "%s", text);
    return written >= 0 && (size_t)written < out_size - used;
}

static bool append_cmd_path(char *out, size_t out_size, const char *path) {
    return append_cmd(out, out_size, " '") && append_cmd(out, out_size, path) && append_cmd(out, out_size, "'");
}

static bool compile_wasm_module(const char *source_path, const char *wasm_path) {
    const char *emcc = getenv("APG_WASM_EMCC");
    if (!emcc || emcc[0] == '\0')
        return false;
    const char *source_root = getenv("APG_WASM_SOURCE_ROOT");
    if (!source_root || source_root[0] == '\0')
        source_root = ".";

    char command[32768] = {0};
    if (!append_cmd(command, sizeof(command), emcc))
        return false;
    if (!append_cmd(command, sizeof(command), " -std=c11 -O2 -D_GNU_SOURCE -DM_PI=3.14159265358979323846"))
        return false;
    if (!append_cmd(command, sizeof(command), " -DM_SQRT1_2=0.70710678118654752440 -I"))
        return false;
    if (!append_cmd_path(command, sizeof(command), source_root))
        return false;
    if (!append_cmd(command, sizeof(command), "/inc -I"))
        return false;
    if (!append_cmd_path(command, sizeof(command), source_root))
        return false;
    if (!append_cmd(command, sizeof(command), "/inc/rte -I"))
        return false;
    if (!append_cmd_path(command, sizeof(command), source_root))
        return false;
    if (!append_cmd(command, sizeof(command), " -s STANDALONE_WASM=1 -s EXPORTED_FUNCTIONS="))
        return false;
    if (!append_cmd(
            command, sizeof(command),
            "_apg_wasm_project_init,_apg_wasm_project_process_block,_apg_wasm_project_set_param,"
            "_apg_wasm_project_input_ptr,_apg_wasm_project_output_ptr,_apg_wasm_project_block_frames"
        ))
        return false;
    if (!append_cmd(command, sizeof(command), " -Wl,--no-entry"))
        return false;
    if (!append_cmd_path(command, sizeof(command), source_path))
        return false;
    if (!append_cmd(command, sizeof(command), " "))
        return false;
    char rooted_path[512];
    int  written = snprintf(rooted_path, sizeof(rooted_path), "%s/src/rte/atom_thunk.c", source_root);
    if (written < 0 || (size_t)written >= sizeof(rooted_path))
        return false;
    if (!append_cmd_path(command, sizeof(command), rooted_path))
        return false;
    static const char *const atom_sources[] = {
        "src/atom/amplitude/amplitude_accumulate.c",
        "src/atom/amplitude/amplitude_add.c",
        "src/atom/amplitude/amplitude_clip_hard.c",
        "src/atom/amplitude/amplitude_clip_soft.c",
        "src/atom/amplitude/amplitude_divide.c",
        "src/atom/amplitude/amplitude_latch.c",
        "src/atom/amplitude/amplitude_multiply.c",
        "src/atom/amplitude/amplitude_normalize.c",
        "src/atom/amplitude/amplitude_smooth.c",
        "src/atom/amplitude/amplitude_subtract.c",
        "src/atom/delay/delay_fractional.c",
        "src/atom/delay/delay_line.c",
        "src/atom/delay/delay_tap_feedback.c",
        "src/atom/delay/delay_tap_feedforward.c",
        "src/atom/delay/delay_unit.c",
        "src/atom/detect/detect_autocorrelate.c",
        "src/atom/detect/detect_envelope.c",
        "src/atom/detect/detect_peak.c",
        "src/atom/detect/detect_pitch.c",
        "src/atom/detect/detect_rms.c",
        "src/atom/detect/detect_slope.c",
        "src/atom/detect/detect_threshold.c",
        "src/atom/detect/detect_zero_crossing.c",
        "src/atom/filter/filter_allpass.c",
        "src/atom/filter/filter_biquad.c",
        "src/atom/filter/filter_comb_fb.c",
        "src/atom/filter/filter_comb_ff.c",
        "src/atom/filter/filter_dc_block.c",
        "src/atom/filter/filter_differentiate.c",
        "src/atom/filter/filter_fir.c",
        "src/atom/filter/filter_integrate.c",
        "src/atom/frequency/frequency_fft.c",
        "src/atom/frequency/frequency_ifft.c",
        "src/atom/frequency/frequency_multiply.c",
        "src/atom/frequency/frequency_overlap_add.c",
        "src/atom/frequency/frequency_overlap_save.c",
        "src/atom/frequency/frequency_quantize.c",
        "src/atom/frequency/frequency_shift.c",
        "src/atom/frequency/frequency_window.c",
        "src/atom/generation/generation_dc.c",
        "src/atom/generation/generation_envelope.c",
        "src/atom/generation/generation_impulse.c",
        "src/atom/generation/generation_lfo.c",
        "src/atom/generation/generation_noise.c",
        "src/atom/generation/generation_oscillator.c",
        "src/atom/interpolation/interpolation_cubic.c",
        "src/atom/interpolation/interpolation_lagrange.c",
        "src/atom/interpolation/interpolation_linear.c",
        "src/atom/interpolation/interpolation_sinc.c",
        "src/atom/mix/mix_crossfade.c",
        "src/atom/mix/mix_decode_ms.c",
        "src/atom/mix/mix_encode_ms.c",
        "src/atom/mix/mix_matrix.c",
        "src/atom/mix/mix_pan_stereo.c",
        "src/atom/mix/mix_wet_dry.c",
        "src/atom/modulation/modulation_amplitude.c",
        "src/atom/modulation/modulation_frequency.c",
        "src/atom/modulation/modulation_phase.c",
        "src/atom/modulation/modulation_ring.c",
        "src/atom/modulation/modulation_scrub.c",
        "src/atom/nonlinear/nonlinear_bitcrush.c",
        "src/atom/nonlinear/nonlinear_sample_hold.c",
        "src/atom/nonlinear/nonlinear_waveshape.c",
        "src/atom/source/source_antialias.c",
        "src/atom/source/source_antiimage.c",
        "src/atom/source/source_convert_format.c",
        "src/atom/source/source_downsample.c",
        "src/atom/source/source_upsample.c",
    };
    for (size_t i = 0; i < sizeof(atom_sources) / sizeof(atom_sources[0]); i++) {
        written = snprintf(rooted_path, sizeof(rooted_path), "%s/%s", source_root, atom_sources[i]);
        if (written < 0 || (size_t)written >= sizeof(rooted_path))
            return false;
        if (!append_cmd(command, sizeof(command), " ") || !append_cmd_path(command, sizeof(command), rooted_path))
            return false;
    }
    if (!append_cmd(command, sizeof(command), " -lm -o") || !append_cmd_path(command, sizeof(command), wasm_path))
        return false;
    return system(command) == 0;
}

static int export_wasm_realtime(const char *project_path, const char *out_dir, const wasm_export_options_t *options) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(project_path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char                         *unit_reason = NULL;
    const apg_project_v2_loaded_unit_t *unsupported = first_unsupported_unit(&project, "wasm_realtime", &unit_reason);
    if (unsupported) {
        uc_error_set(
            &err, UC_E_TYPE, (uc_loc){0, 0}, "unit '%s' %s", unsupported->id,
            unit_reason ? unit_reason : "does not support target profile"
        );
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char *unsupported_atom = first_unsupported_atom(&compiled.plan, "wasm_realtime");
    if (unsupported_atom) {
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "atom '%s' does not support wasm_realtime", unsupported_atom);
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    uc_arena          registry_arena = {0};
    apg_v2_registry_t registry       = {0};
    status                           = apg_v2_registry_build_with_growth(
        &compiled.plan, options->block_frames, (float)options->sample_rate, &registry_arena, &registry, &err
    );
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&arena);
        return rc;
    }

    if (!validate_export_output_dir(out_dir, &err)) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    char manifest_path[512];
    char js_path[512];
    char adapter_path[512];
    char processor_path[512];
    char header_path[512];
    char source_path[512];
    char wasm_path[512];
    if (!join_path(manifest_path, sizeof(manifest_path), out_dir, "apg_project_wasm.json") ||
        !join_path(js_path, sizeof(js_path), out_dir, "apg_project_wasm.mjs") ||
        !join_path(adapter_path, sizeof(adapter_path), out_dir, "apg_project_wasm_adapter.mjs") ||
        !join_path(processor_path, sizeof(processor_path), out_dir, "apg_project_wasm_processor.js") ||
        !join_path(header_path, sizeof(header_path), out_dir, "apg_project_wasm.h") ||
        !join_path(source_path, sizeof(source_path), out_dir, "apg_project_wasm.c") ||
        !join_path(wasm_path, sizeof(wasm_path), out_dir, "apg_project_wasm.wasm")) {
        uc_error_set(&err, UC_E_RANGE, (uc_loc){0, 0}, "export output path is too long");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    bool wasm_module_available = false;
    if (!write_wasm_header(header_path, &registry) || !write_wasm_source(source_path, &project, &registry)) {
        uc_error_set(&err, UC_E_IO, (uc_loc){0, 0}, "failed to write wasm_realtime C bundle");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }
    wasm_module_available = compile_wasm_module(source_path, wasm_path);

    if (!write_wasm_runtime_manifest(manifest_path, &project, &registry, options, wasm_module_available) ||
        !write_wasm_runtime_js(js_path, &project, &registry, options, wasm_module_available) ||
        !write_wasm_runtime_adapter_js(adapter_path, &registry) || !write_wasm_runtime_processor_js(processor_path)) {
        uc_error_set(&err, UC_E_IO, (uc_loc){0, 0}, "failed to write wasm_realtime export files");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "wasm_realtime", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.export.v2\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, project_path);
    fputs(",\"target\":\"wasm_realtime\",\"out_dir\":", stdout);
    write_json_string(stdout, out_dir);
    fputs(
        ",\"status\":\"generated\",\"files\":[\"apg_project_wasm.json\",\"apg_project_wasm.mjs\","
        "\"apg_project_wasm_adapter.mjs\",\"apg_project_wasm_processor.js\","
        "\"apg_project_wasm.h\",\"apg_project_wasm.c\"",
        stdout
    );
    if (wasm_module_available)
        fputs(",\"apg_project_wasm.wasm\"", stdout);
    fputs("],\"wasm_module_available\":", stdout);
    fputs(wasm_module_available ? "true" : "false", stdout);
    fputs(",\"nodes\":", stdout);
    fprintf(stdout, "%zu,\"schedule\":%zu", registry.nodes_len, registry.schedule_len);
    fprintf(
        stdout, ",\"execution\":{\"sample_rate\":%u,\"block_frames\":%u,\"atom_calls_per_block\":%zu,",
        options->sample_rate, options->block_frames, registry.schedule_len
    );
    fprintf(
        stdout, "\"atom_calls_per_second\":%zu}}\n",
        registry.schedule_len * options->sample_rate / options->block_frames
    );

    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

static int export_m7_static(const char *project_path, const char *out_dir, const m7_export_options_t *options) {
    uc_arena arena;
    if (uc_arena_init(&arena, 2 * 1024 * 1024) != 0) {
        uc_error err = {.status = UC_E_OOM};
        return write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
    }

    apg_project_v2_resolved_t project;
    apg_project_v2_compiled_t compiled;
    uc_error                  err    = {0};
    uc_status                 status = load_compile_project(project_path, &arena, &project, &compiled, &err);
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char                         *unit_reason = NULL;
    const apg_project_v2_loaded_unit_t *unsupported = first_unsupported_unit(&project, "m7_static", &unit_reason);
    if (unsupported) {
        uc_error_set(
            &err, UC_E_TYPE, (uc_loc){0, 0}, "unit '%s' %s", unsupported->id,
            unit_reason ? unit_reason : "does not support m7_static"
        );
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    const char *unsupported_atom = first_unsupported_atom(&compiled.plan, "m7_static");
    if (unsupported_atom) {
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "atom '%s' does not support m7_static", unsupported_atom);
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }

    uc_arena          registry_arena = {0};
    apg_v2_registry_t registry       = {0};
    status                           = apg_v2_registry_build_with_growth(
        &compiled.plan, options->block_frames, (float)options->sample_rate, &registry_arena, &registry, &err
    );
    if (status != UC_OK) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&arena);
        return rc;
    }
    m7_memory_manifest_t memory = m7_memory_manifest(&registry);
    if (options->has_static_ram_budget && memory.static_ram_bytes > options->static_ram_budget) {
        uc_error_set(
            &err, UC_E_RANGE, (uc_loc){0, 0}, "m7_static static RAM budget exceeded: %zu > %zu bytes",
            memory.static_ram_bytes, options->static_ram_budget
        );
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    if (!validate_export_output_dir(out_dir, &err)) {
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    char header_path[512];
    char source_path[512];
    if (!join_path(header_path, sizeof(header_path), out_dir, "apg_project_m7.h") ||
        !join_path(source_path, sizeof(source_path), out_dir, "apg_project_m7.c")) {
        uc_error_set(&err, UC_E_RANGE, (uc_loc){0, 0}, "export output path is too long");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    if (!write_m7_header(header_path, &registry, &memory, options) ||
        !write_m7_source(source_path, &project, &registry)) {
        uc_error_set(&err, UC_E_IO, (uc_loc){0, 0}, "failed to write m7_static export files");
        int rc = write_cli_error(stdout, "apg.project.export.v2", project_path, "m7_static", &err);
        uc_arena_free(&registry_arena);
        uc_arena_free(&arena);
        return rc;
    }

    fputs("{\"schema\":\"apg.project.export.v2\",\"ok\":true,\"file\":", stdout);
    write_json_string(stdout, project_path);
    fputs(",\"target\":\"m7_static\",\"out_dir\":", stdout);
    write_json_string(stdout, out_dir);
    fprintf(
        stdout,
        ",\"files\":[\"apg_project_m7.h\",\"apg_project_m7.c\"],\"nodes\":%zu,\"schedule\":%zu,"
        "\"memory\":{\"block_frames\":%u,\"signal_buffer_bytes\":%zu,\"param_bytes\":%zu,"
        "\"schedule_bytes\":%zu,\"atom_call_bytes\":%zu,\"signal_array_pointer_bytes\":%zu,"
        "\"atom_storage_bytes\":%zu,\"state_buffer_bytes\":%zu,"
        "\"static_ram_bytes\":%zu,\"cache_line_bytes\":%u},\"execution\":{\"sample_rate\":%u,\"block_frames\":%u,"
        "\"atom_calls_per_block\":%zu,\"atom_calls_per_second\":%zu}}\n",
        registry.nodes_len, registry.schedule_len, options->block_frames, memory.signal_buffer_bytes,
        memory.param_bytes, memory.schedule_bytes, memory.atom_call_bytes, memory.signal_array_pointer_bytes,
        memory.atom_storage_bytes, memory.state_buffer_bytes, memory.static_ram_bytes, options->cache_line_bytes,
        options->sample_rate, options->block_frames, registry.schedule_len,
        registry.schedule_len * options->sample_rate / options->block_frames
    );
    uc_arena_free(&registry_arena);
    uc_arena_free(&arena);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3)
        return usage(argv[0]);

    if (strcmp(argv[1], "validate") == 0) {
        if (argc != 4)
            return usage(argv[0]);
        if (strcmp(argv[2], "unit") == 0) {
            apg_v2_json_write_validate_unit(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        if (strcmp(argv[2], "project") == 0) {
            apg_v2_json_write_validate_project(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        return usage(argv[0]);
    }

    if (strcmp(argv[1], "inspect") == 0) {
        if (strcmp(argv[2], "atoms") == 0) {
            if (argc != 3)
                return usage(argv[0]);
            apg_atom_catalog_write_json(stdout);
            return 0;
        }
        if (argc != 4)
            return usage(argv[0]);
        if (strcmp(argv[2], "unit") == 0) {
            apg_v2_json_write_inspect_unit(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        if (strcmp(argv[2], "project") == 0) {
            apg_v2_json_write_inspect_project(stdout, argv[3]);
            fputc('\n', stdout);
            return 0;
        }
        return usage(argv[0]);
    }

    if (strcmp(argv[1], "render") == 0) {
        if (argc != 4 || strcmp(argv[2], "project") != 0)
            return usage(argv[0]);
        apg_v2_json_write_render_project(stdout, argv[3]);
        fputc('\n', stdout);
        return 0;
    }

    if (strcmp(argv[1], "benchmark") == 0) {
        if (argc != 4 || strcmp(argv[2], "project") != 0)
            return usage(argv[0]);
        return benchmark_project(argv[3]);
    }

    if (strcmp(argv[1], "export") == 0) {
        if (argc < 6 || strcmp(argv[2], "--target") != 0)
            return usage(argv[0]);
        if (strcmp(argv[3], "wasm_realtime") == 0) {
            wasm_export_options_t options = {
                .block_frames = APG_WASM_DEFAULT_BLOCK_FRAMES,
                .sample_rate  = APG_WASM_DEFAULT_SAMPLE_RATE,
            };
            int index = 4;
            while (index + 2 < argc && strncmp(argv[index], "--", 2) == 0) {
                if (strcmp(argv[index], "--block-frames") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.block_frames))
                        return usage(argv[0]);
                } else if (strcmp(argv[index], "--sample-rate") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.sample_rate))
                        return usage(argv[0]);
                } else {
                    return usage(argv[0]);
                }
                index += 2;
            }
            if (argc - index != 2)
                return usage(argv[0]);
            return export_wasm_realtime(argv[index], argv[index + 1], &options);
        }
        if (strcmp(argv[3], "m7_static") == 0) {
            m7_export_options_t options = {
                .block_frames     = APG_M7_DEFAULT_BLOCK_FRAMES,
                .sample_rate      = APG_M7_DEFAULT_SAMPLE_RATE,
                .cache_line_bytes = APG_M7_DEFAULT_CACHE_LINE,
            };
            int index = 4;
            while (index + 2 < argc && strncmp(argv[index], "--", 2) == 0) {
                if (strcmp(argv[index], "--max-static-ram") == 0) {
                    if (!parse_size_arg(argv[index + 1], &options.static_ram_budget))
                        return usage(argv[0]);
                    options.has_static_ram_budget = true;
                } else if (strcmp(argv[index], "--block-frames") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.block_frames))
                        return usage(argv[0]);
                } else if (strcmp(argv[index], "--sample-rate") == 0) {
                    if (!parse_uint32_arg(argv[index + 1], &options.sample_rate))
                        return usage(argv[0]);
                } else if (strcmp(argv[index], "--cache-line-bytes") == 0) {
                    if (!parse_alignment_arg(argv[index + 1], &options.cache_line_bytes))
                        return usage(argv[0]);
                } else {
                    return usage(argv[0]);
                }
                index += 2;
            }
            if (argc - index != 2)
                return usage(argv[0]);
            return export_m7_static(argv[index], argv[index + 1], &options);
        }
        uc_error err = {0};
        uc_error_set(&err, UC_E_TYPE, (uc_loc){0, 0}, "unsupported export target");
        return write_cli_error(stdout, "apg.project.export.v2", argv[4], argv[3], &err);
    }

    return usage(argv[0]);
}
