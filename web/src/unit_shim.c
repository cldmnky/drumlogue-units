/**
 * @file unit_shim.c
 * @brief Host shim that exposes a logue SDK unit to the web emulator.
 *
 * The unit's sources are compiled into the same WebAssembly module as this
 * file, so the unit_* callbacks are called directly - no dynamic loading.
 * Everything the unit reads from the runtime descriptor is described here, and
 * the unit's header is turned into a JSON manifest so the browser can build a
 * UI without knowing anything about the unit.
 *
 * The JS side owns the audio: it calls host_render() with at most
 * frames_per_buffer frames, then copies host_out_ptr() into the AudioWorklet's
 * output. Input audio (for effect units) goes through host_in_ptr().
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unit.h"

#define HOST_MAX_FRAMES 256
#define HOST_OUT_CHANNELS 2
#define HOST_MANIFEST_CAP 16384
#define HOST_STRING_CAP 128

static unit_runtime_desc_t s_desc;

/** Whether this unit processes an input (a reverb, delay or master effect). */
static int HostNeedsInput(void) {
    switch (unit_header.target & 0xFFu) {
        case k_unit_module_modfx:
        case k_unit_module_delfx:
        case k_unit_module_revfx:
        case k_unit_module_masterfx:
            return 1;
        default:
            return 0;
    }
}
static float s_out[HOST_MAX_FRAMES * HOST_OUT_CHANNELS];
static float s_in[HOST_MAX_FRAMES * HOST_OUT_CHANNELS];
static int s_frames_per_buffer = 64;
static int s_initialised = 0;
static char s_manifest[HOST_MANIFEST_CAP];
static char s_string[HOST_STRING_CAP];

/* The SDK's sample API: the emulator has no sample banks yet. */
static uint8_t HostSampleBanks(void) { return 0; }
static uint8_t HostSamplesForBank(uint8_t bank) {
    (void)bank;
    return 0;
}
static const sample_wrapper_t *HostSample(uint8_t bank, uint8_t index) {
    (void)bank;
    (void)index;
    return 0;
}

/* --- bounds-checked string building --------------------------------------- */

typedef struct {
    char *cursor;
    char *end;
} Writer;

static void WriterInit(Writer *writer, char *buffer, int capacity) {
    writer->cursor = buffer;
    writer->end = buffer + capacity;
    if (capacity > 0) buffer[0] = '\0';
}

static int WriterRoom(Writer *writer) { return writer->cursor < writer->end; }

static void WriterText(Writer *writer, const char *format, ...) {
    if (!WriterRoom(writer)) return;
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(writer->cursor, (size_t)(writer->end - writer->cursor),
                                  format, args);
    va_end(args);
    if (written > 0) {
        writer->cursor += written;
        if (writer->cursor > writer->end) writer->cursor = writer->end;
    }
}

static void WriterJsonString(Writer *writer, const char *text) {
    WriterText(writer, "\"");
    for (const char *p = text; p != NULL && *p != '\0' && WriterRoom(writer); p++) {
        const unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            WriterText(writer, "\\%c", c);
        } else if (c < 0x20) {
            WriterText(writer, "\\u%04x", c);
        } else {
            WriterText(writer, "%c", c);
        }
    }
    WriterText(writer, "\"");
}

static const char *ParamTypeName(uint8_t type) {
    switch (type) {
        case k_unit_param_type_none: return "none";
        case k_unit_param_type_percent: return "percent";
        case k_unit_param_type_db: return "db";
        case k_unit_param_type_cents: return "cents";
        case k_unit_param_type_semi: return "semi";
        case k_unit_param_type_oct: return "oct";
        case k_unit_param_type_hertz: return "hertz";
        case k_unit_param_type_khertz: return "khertz";
        case k_unit_param_type_bpm: return "bpm";
        case k_unit_param_type_msec: return "msec";
        case k_unit_param_type_sec: return "sec";
        case k_unit_param_type_enum: return "enum";
        case k_unit_param_type_strings: return "strings";
        case k_unit_param_type_bitmaps: return "bitmaps";
        case k_unit_param_type_drywet: return "drywet";
        case k_unit_param_type_pan: return "pan";
        case k_unit_param_type_spread: return "spread";
        case k_unit_param_type_onoff: return "onoff";
        case k_unit_param_type_midi_note: return "midi_note";
        default: return "unknown";
    }
}

/* --- exported host API ---------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Bring the unit up.
 * @return the unit's own init result (0 = ok), or -1 for a bad argument.
 */
int host_init(int samplerate, int frames_per_buffer) {
    if (samplerate <= 0 || frames_per_buffer < 1 ||
        frames_per_buffer > HOST_MAX_FRAMES) {
        return -100;  // host argument error, outside the SDK's error range
    }
    memset(&s_desc, 0, sizeof(s_desc));
    s_desc.target = unit_header.target;
    s_desc.api = UNIT_API_VERSION;
    s_desc.samplerate = (uint32_t)samplerate;
    s_desc.frames_per_buffer = (uint16_t)frames_per_buffer;
    s_desc.input_channels = (uint8_t)(HostNeedsInput() ? HOST_OUT_CHANNELS : 0);
    s_desc.output_channels = HOST_OUT_CHANNELS;
    s_desc.get_num_sample_banks = HostSampleBanks;
    s_desc.get_num_samples_for_bank = HostSamplesForBank;
    s_desc.get_sample = HostSample;
    s_frames_per_buffer = frames_per_buffer;
    s_initialised = 0;
    const int8_t result = unit_init(&s_desc);
    if (result == 0) s_initialised = 1;
    return (int)result;
}

void host_teardown(void) {
    if (!s_initialised) return;
    unit_teardown();
    s_initialised = 0;
}

/** Render up to frames_per_buffer frames; the result lands at host_out_ptr(). */
void host_render(int frames) {
    if (!s_initialised || frames <= 0) return;
    if (frames > s_frames_per_buffer) frames = s_frames_per_buffer;
    unit_render(s_in, s_out, (uint32_t)frames);
}

float *host_out_ptr(void) { return s_out; }
float *host_in_ptr(void) { return s_in; }
int host_channels(void) { return HOST_OUT_CHANNELS; }

void host_note_on(int note, int velocity) {
    if (s_initialised) unit_note_on((uint8_t)note, (uint8_t)velocity);
}

void host_note_off(int note) {
    if (s_initialised) unit_note_off((uint8_t)note);
}

void host_all_note_off(void) {
    if (s_initialised) unit_all_note_off();
}

void host_set_param(int id, int value) {
    if (s_initialised) unit_set_param_value((uint8_t)id, (int32_t)value);
}

int host_get_param(int id) {
    return s_initialised ? (int)unit_get_param_value((uint8_t)id) : 0;
}

void host_load_preset(int index) {
    if (s_initialised) unit_load_preset((uint8_t)index);
}

int host_get_preset_index(void) {
    return s_initialised ? (int)unit_get_preset_index() : 0;
}

/** Tempo in BPM as a plain number; the SDK wants Q16.16. */
void host_set_tempo(int bpm) {
    if (s_initialised) unit_set_tempo((uint32_t)bpm << 16);
}

void host_pitch_bend(int bend) {
    if (s_initialised) unit_pitch_bend((uint16_t)bend);
}

void host_reset(void) {
    if (s_initialised) unit_reset();
}

/** Copy a parameter's string value; returns 0 when there is none. */
int host_param_str(int id, int value) {
    s_string[0] = '\0';
    if (!s_initialised) return 0;
    const char *text = unit_get_param_str_value((uint8_t)id, (int32_t)value);
    if (text == NULL) return 0;
    snprintf(s_string, sizeof(s_string), "%s", text);
    return (int)strlen(s_string);
}

const char *host_string_ptr(void) { return s_string; }

const char *host_manifest_ptr(void) { return s_manifest; }

/**
 * @brief Describe the unit as JSON: name, parameter ranges/types/value tables
 *        and presets. The browser builds its UI from this.
 * @return the length of the written JSON.
 */
int host_manifest(void) {
    static const int kMaxValues = 256;
    Writer writer;
    WriterInit(&writer, s_manifest, HOST_MANIFEST_CAP);

    WriterText(&writer, "{\"name\":");
    WriterJsonString(&writer, unit_header.name);
    WriterText(&writer, ",\"target\":%u,\"api\":%u", (unsigned)unit_header.target,
               (unsigned)unit_header.api);
    WriterText(&writer, ",\"numParams\":%u,\"numPresets\":%u",
               (unsigned)unit_header.num_params, (unsigned)unit_header.num_presets);
    WriterText(&writer, ",\"moduleType\":%u,\"module\":", (unsigned)(unit_header.target & 0xFFu));
    WriterJsonString(&writer, HostNeedsInput() ? "effect" : "synth");

    WriterText(&writer, ",\"params\":[");
    for (uint32_t i = 0; i < unit_header.num_params; i++) {
        const unit_param_t *param = &unit_header.params[i];
        if (i > 0) WriterText(&writer, ",");
        WriterText(&writer, "{\"index\":%u,\"name\":", (unsigned)i);
        WriterJsonString(&writer, param->name);
        WriterText(&writer,
                   ",\"min\":%d,\"max\":%d,\"center\":%d,\"init\":%d,\"type\":%u,"
                   "\"typeName\":",
                   (int)param->min, (int)param->max, (int)param->center,
                   s_initialised ? (int)unit_get_param_value((uint8_t)i) : (int)param->init,
                   (unsigned)param->type);
        WriterJsonString(&writer, ParamTypeName(param->type));

        const int has_values = param->type == k_unit_param_type_strings ||
                              param->type == k_unit_param_type_enum;
        if (has_values && s_initialised) {
            WriterText(&writer, ",\"values\":[");
            const int count = (int)param->max - (int)param->min + 1;
            int emitted = 0;
            for (int value = param->min; value <= param->max && emitted < kMaxValues; value++) {
                // A NULL label still occupies a slot: the UI maps a value to
                // values[value - valueMin], so skipping one would shift the rest.
                const char *text = unit_get_param_str_value((uint8_t)i, (int32_t)value);
                if (emitted > 0) WriterText(&writer, ",");
                WriterJsonString(&writer, text != NULL ? text : "");
                emitted++;
            }
            WriterText(&writer, "],\"valueMin\":%d,\"valueCount\":%d", (int)param->min,
                       count);
        }
        WriterText(&writer, "}");
    }
    WriterText(&writer, "],\"presets\":[");
    for (uint32_t i = 0; i < unit_header.num_presets; i++) {
        const char *name = unit_get_preset_name((uint8_t)i);
        if (i > 0) WriterText(&writer, ",");
        WriterText(&writer, "{\"index\":%u,\"name\":", (unsigned)i);
        WriterJsonString(&writer, name != NULL ? name : "");
        WriterText(&writer, "}");
    }
    WriterText(&writer, "]}");

    return (int)strlen(s_manifest);
}

#ifdef __cplusplus
}
#endif
