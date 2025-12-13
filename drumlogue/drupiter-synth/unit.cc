/**
 * @file unit.cc
 * @brief drumlogue SDK unit interface for Drupiter synth
 *
 * Based on Bristol Jupiter-8 emulation
 * Jupiter-8 inspired monophonic synthesizer
 */

#include <cstddef>
#include <cstdint>

#include "unit.h"           // Common definitions for all units
#include "drupiter_synth.h" // Main synthesizer class

static DrupiterSynth s_synth_instance;      // Synth instance
static unit_runtime_desc_t s_runtime_desc;  // Cached runtime descriptor

// ---- Callback entry points from drumlogue runtime ----------------------------------------------

__unit_callback int8_t unit_init(const unit_runtime_desc_t* desc) {
    if (!desc)
        return k_unit_err_undef;

    if (desc->target != unit_header.target)
        return k_unit_err_target;

    if (!UNIT_API_IS_COMPAT(desc->api))
        return k_unit_err_api_version;

    s_runtime_desc = *desc;  // Cache runtime descriptor

    return s_synth_instance.Init(desc);
}

__unit_callback void unit_teardown() {
    s_synth_instance.Teardown();
}

__unit_callback void unit_reset() {
    s_synth_instance.Reset();
}

__unit_callback void unit_resume() {
    s_synth_instance.Resume();
}

__unit_callback void unit_suspend() {
    s_synth_instance.Suspend();
}

__unit_callback void unit_render(const float* in, float* out, uint32_t frames) {
    (void)in;  // Drupiter is a synth, doesn't process external input
    s_synth_instance.Render(out, frames);
}

__unit_callback void unit_set_param_value(uint8_t id, int32_t value) {
    s_synth_instance.SetParameter(id, value);
}

__unit_callback int32_t unit_get_param_value(uint8_t id) {
    return s_synth_instance.GetParameter(id);
}

__unit_callback const char* unit_get_param_str_value(uint8_t id, int32_t value) {
    return s_synth_instance.GetParameterStr(id, value);
}

__unit_callback const uint8_t* unit_get_param_bmp_value(uint8_t id, int32_t value) {
    (void)id;
    (void)value;
    return nullptr;  // No bitmap parameters in Drupiter
}

__unit_callback void unit_set_tempo(uint32_t tempo) {
    (void)tempo;  // Tempo not used in basic implementation
}

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    s_synth_instance.NoteOn(note, velocity);
}

__unit_callback void unit_note_off(uint8_t note) {
    s_synth_instance.NoteOff(note);
}

__unit_callback void unit_gate_on(uint8_t velocity) {
    // Use last note or default C4 (60)
    s_synth_instance.NoteOn(60, velocity);
}

__unit_callback void unit_gate_off() {
    s_synth_instance.AllNoteOff();
}

__unit_callback void unit_all_note_off() {
    s_synth_instance.AllNoteOff();
}

__unit_callback void unit_pitch_bend(uint16_t bend) {
    (void)bend;  // Pitch bend not implemented in basic version
}

__unit_callback void unit_channel_pressure(uint8_t pressure) {
    (void)pressure;  // Channel pressure not implemented
}

__unit_callback void unit_aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;  // Aftertouch not implemented
}

__unit_callback void unit_load_preset(uint8_t idx) {
    s_synth_instance.LoadPreset(idx);
}

__unit_callback uint8_t unit_get_preset_index() {
    return 0;  // Return current preset index (TODO: track in synth)
}

__unit_callback const char* unit_get_preset_name(uint8_t idx) {
    static const char* preset_names[] = {
        "Init",
        "Bass",
        "Lead",
        "Pad",
        "Brass",
        "Strings"
    };
    
    if (idx < 6) {
        return preset_names[idx];
    }
    return "Unknown";
}
