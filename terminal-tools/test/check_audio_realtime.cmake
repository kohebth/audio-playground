file(READ "${AUDIO_SOURCE}" AUDIO_CONTENT)

string(FIND "${AUDIO_CONTENT}" "class RealtimeProjectEngine" ENGINE_CLASS_START)
if (ENGINE_CLASS_START EQUAL -1)
    message(FATAL_ERROR "Could not isolate RealtimeProjectEngine")
endif ()
string(SUBSTRING "${AUDIO_CONTENT}" ${ENGINE_CLASS_START} -1 ENGINE_CLASS)
string(FIND "${ENGINE_CLASS}" "bool process(const float *input, float *output, std::uint32_t frames) noexcept" PROCESS_START)
string(FIND "${ENGINE_CLASS}" "    void service()" PROCESS_END)
if (PROCESS_START EQUAL -1 OR PROCESS_END EQUAL -1 OR PROCESS_END LESS PROCESS_START)
    message(FATAL_ERROR "Could not isolate RealtimeProjectEngine::process")
endif ()
math(EXPR PROCESS_LENGTH "${PROCESS_END} - ${PROCESS_START}")
string(SUBSTRING "${ENGINE_CLASS}" ${PROCESS_START} ${PROCESS_LENGTH} PROCESS_BODY)

string(FIND "${AUDIO_CONTENT}" "    void apply_controls() noexcept" CONTROLS_START)
if (CONTROLS_START EQUAL -1)
    message(FATAL_ERROR "Could not isolate CompiledProjectGraph::apply_controls")
endif ()
string(SUBSTRING "${AUDIO_CONTENT}" ${CONTROLS_START} -1 CONTROLS_TAIL)
string(FIND "${CONTROLS_TAIL}" "    uc_arena" CONTROLS_LENGTH)
if (CONTROLS_LENGTH EQUAL -1)
    message(FATAL_ERROR "Could not find the end of CompiledProjectGraph::apply_controls")
endif ()
string(SUBSTRING "${CONTROLS_TAIL}" 0 ${CONTROLS_LENGTH} CONTROLS_BODY)

string(FIND "${AUDIO_CONTENT}" "    static void data_callback(" CALLBACK_START)
string(FIND "${AUDIO_CONTENT}" "    static void notification_callback(" CALLBACK_END)
if (CALLBACK_START EQUAL -1 OR CALLBACK_END EQUAL -1 OR CALLBACK_END LESS CALLBACK_START)
    message(FATAL_ERROR "Could not isolate the miniaudio data callback")
endif ()
math(EXPR CALLBACK_LENGTH "${CALLBACK_END} - ${CALLBACK_START}")
string(SUBSTRING "${AUDIO_CONTENT}" ${CALLBACK_START} ${CALLBACK_LENGTH} CALLBACK_BODY)

set(FORBIDDEN_REALTIME_PATTERNS
        "malloc[ \t]*\\("
        "calloc[ \t]*\\("
        "realloc[ \t]*\\("
        "free[ \t]*\\("
        "new[ \t]+"
        "delete[ \t]+"
        "make_unique[ \t]*\\("
        "make_shared[ \t]*\\("
        "scoped_lock"
        "unique_lock"
        "lock_guard"
        "mutex"
        "filesystem"
        "snprintf[ \t]*\\("
        "to_string[ \t]*\\("
        "parse[ \t]*\\("
        "compile[ \t]*\\("
        "sleep"
        "wait[ \t]*\\("
)

foreach (REALTIME_BODY IN ITEMS PROCESS_BODY CONTROLS_BODY CALLBACK_BODY)
    foreach (FORBIDDEN_PATTERN IN LISTS FORBIDDEN_REALTIME_PATTERNS)
        if ("${${REALTIME_BODY}}" MATCHES "${FORBIDDEN_PATTERN}")
            message(FATAL_ERROR "Terminal audio callback contains forbidden pattern: ${FORBIDDEN_PATTERN}")
        endif ()
    endforeach ()
endforeach ()

message(STATUS "Terminal audio real-time boundary is clean")
