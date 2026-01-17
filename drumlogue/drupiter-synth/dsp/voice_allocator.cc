/*
 * File: voice_allocator.cc
 * Project: Drupiter-Synth Hoover Synthesis Implementation
 * 
 * Description: Drupiter-specific voice management using common core allocator
 */

#include "voice_allocator.h"
#include "../../common/midi_helper.h"
#include <cstring>
#include <algorithm>

#ifdef TEST
#include <cstdio>
#endif

// Enable USE_NEON for ARM NEON optimizations
#ifndef TEST
#ifdef __ARM_NEON
#define USE_NEON 1
#endif
#endif

// Disable NEON DSP when testing on host
#ifndef TEST
#define NEON_DSP_NS drupiter
#include "../../common/neon_dsp.h"
#else
// Stub implementation for testing
namespace drupiter {
namespace neon {
inline void ClearStereoBuffers(float* left, float* right, uint32_t frames) {
	for (uint32_t i = 0; i < frames; ++i) { 
		left[i] = 0.0f; 
		right[i] = 0.0f; 
	}
}
}
}
#endif

namespace dsp {

// ============================================================================
// Voice Implementation
// ============================================================================

void Voice::Init(float sample_rate) {
	dco1.Init(sample_rate);
	dco2.Init(sample_rate);
	vcf.Init(sample_rate);
	env_amp.Init(sample_rate);
	env_filter.Init(sample_rate);
	env_pitch.Init(sample_rate);
	Reset();
}

void Voice::Reset() {
	active = false;
	midi_note = 0;
	velocity = 0.0f;
	pitch_hz = 0.0f;
	note_on_time = 0;
	env_amp.Reset();
	env_filter.Reset();
	env_pitch.Reset();
}

// ============================================================================
// VoiceAllocator Implementation
// ============================================================================

VoiceAllocator::VoiceAllocator()
	: max_voices_(DRUPITER_MAX_VOICES)
	, active_voices_(0)
	, round_robin_index_(0)
	, timestamp_(0)
	, active_voice_list_()
	, num_active_voices_(0)
	, mode_(SYNTH_MODE_MONOPHONIC)
	, allocation_strategy_(ALLOC_ROUND_ROBIN)
	, unison_detune_cents_(10.0f)
	, portamento_time_ms_(0.0f)
	, sample_rate_(48000.0f)
	, core_() {
	for (uint8_t i = 0; i < max_voices_; i++) {
		voices_[i].active = false;
		voices_[i].midi_note = 0;
		voices_[i].velocity = 0.0f;
		voices_[i].pitch_hz = 0.0f;
		voices_[i].note_on_time = 0;
		voices_[i].glide_target_hz = 0.0f;
		voices_[i].glide_increment = 0.0f;
		voices_[i].is_gliding = false;
	}
	memset(active_voice_list_, 0, sizeof(active_voice_list_));
	core_.Init(max_voices_);
}

VoiceAllocator::~VoiceAllocator() {
}

void VoiceAllocator::Init(float sample_rate) {
	sample_rate_ = sample_rate;
	for (uint8_t i = 0; i < max_voices_; i++) {
		voices_[i].Init(sample_rate);
	}
	unison_osc_.Init(sample_rate, max_voices_);
	unison_osc_.SetDetune(unison_detune_cents_);
	unison_osc_.SetStereoSpread(0.7f);
}

void VoiceAllocator::SetMode(SynthMode mode) {
	mode_ = mode;
	switch (mode_) {
		case SYNTH_MODE_MONOPHONIC:
			core_.SetMode(common::VoiceMode::Monophonic);
			break;
		case SYNTH_MODE_POLYPHONIC:
			core_.SetMode(common::VoiceMode::Polyphonic);
			break;
		case SYNTH_MODE_UNISON:
			core_.SetMode(common::VoiceMode::Unison);
			break;
	}
}

void VoiceAllocator::SetAllocationStrategy(VoiceAllocationStrategy strategy) {
	allocation_strategy_ = strategy;
	switch (strategy) {
		case ALLOC_OLDEST_NOTE:
			core_.SetAllocationStrategy(common::VoiceAllocationStrategy::OldestNote);
			break;
		case ALLOC_FIRST_AVAILABLE:
			core_.SetAllocationStrategy(common::VoiceAllocationStrategy::FirstAvailable);
			break;
		case ALLOC_ROUND_ROBIN:
		default:
			core_.SetAllocationStrategy(common::VoiceAllocationStrategy::RoundRobin);
			break;
	}
}

void VoiceAllocator::NoteOn(uint8_t note, uint8_t velocity) {
	timestamp_++;
	const common::NoteOnResult result = core_.NoteOn(note, velocity);
	if (result.voice_index < 0 || result.voice_index >= max_voices_) {
		return;
	}

	Voice* voice = nullptr;
	uint8_t voice_idx = static_cast<uint8_t>(result.voice_index);

	if (mode_ == SYNTH_MODE_UNISON) {
		float frequency = common::MidiHelper::NoteToFreq(note);
		unison_osc_.SetFrequency(frequency);
		voice = &voices_[0];
		voice_idx = 0;
	} else if (mode_ == SYNTH_MODE_MONOPHONIC) {
		voice = &voices_[0];
		voice_idx = 0;
	} else {
		voice = &voices_[voice_idx];
	}

	if (voice) {
		TriggerVoice(voice, note, velocity, result.allow_legato);
		AddActiveVoice(voice_idx);
		core_.SetVoiceActive(voice_idx, true);
	}
}

void VoiceAllocator::NoteOff(uint8_t note) {
	const common::NoteOffResult result = core_.NoteOff(note);

	switch (mode_) {
		case SYNTH_MODE_MONOPHONIC:
			if (voices_[0].active && voices_[0].midi_note == note) {
				if (result.retrigger && result.retrigger_note > 0) {
					TriggerVoice(&voices_[0], result.retrigger_note, 64, true);
				} else {
					voices_[0].env_amp.NoteOff();
					voices_[0].env_filter.NoteOff();
					voices_[0].env_pitch.NoteOff();
				}
			}
			break;

		case SYNTH_MODE_POLYPHONIC:
			for (uint8_t i = 0; i < max_voices_; i++) {
				if (voices_[i].active && voices_[i].midi_note == note) {
					voices_[i].env_amp.NoteOff();
					voices_[i].env_filter.NoteOff();
					voices_[i].env_pitch.NoteOff();
				}
			}
			break;

		case SYNTH_MODE_UNISON:
			if (voices_[0].active && voices_[0].midi_note == note) {
				if (result.retrigger && result.retrigger_note > 0) {
					float frequency = common::MidiHelper::NoteToFreq(result.retrigger_note);
					unison_osc_.SetFrequency(frequency);
					TriggerVoice(&voices_[0], result.retrigger_note, 64, true);
				} else {
					for (uint8_t i = 0; i < max_voices_; i++) {
						if (voices_[i].active) {
							voices_[i].env_amp.NoteOff();
							voices_[i].env_filter.NoteOff();
							voices_[i].env_pitch.NoteOff();
						}
					}
				}
			}
			break;
	}
}

void VoiceAllocator::AllNotesOff() {
	core_.AllNotesOff();
	for (uint8_t i = 0; i < max_voices_; i++) {
		if (voices_[i].active) {
			voices_[i].env_amp.NoteOff();
			voices_[i].env_filter.NoteOff();
			voices_[i].env_pitch.NoteOff();
		}
	}
}

void VoiceAllocator::RenderMonophonic(float* left, float* right, uint32_t frames, const float* /*params*/) {
	Voice& voice = voices_[0];
	if (!voice.active && !voice.env_amp.IsActive()) {
		drupiter::neon::ClearStereoBuffers(left, right, frames);
		return;
	}
	drupiter::neon::ClearStereoBuffers(left, right, frames);
}

void VoiceAllocator::RenderPolyphonic(float* left, float* right, uint32_t frames, const float* /*params*/) {
	drupiter::neon::ClearStereoBuffers(left, right, frames);
	uint8_t active_count = 0;
	for (uint8_t i = 0; i < num_active_voices_; i++) {
		uint8_t v = active_voice_list_[i];
		Voice& voice = voices_[v];
		if (!voice.active && !voice.env_amp.IsActive()) {
			continue;
		}
		active_count++;
	}
	UpdateActiveVoiceList();
	if (active_count > 1) {
		float scale = 1.0f / sqrtf(static_cast<float>(active_count));
		for (uint32_t i = 0; i < frames; i++) {
			left[i] *= scale;
			right[i] *= scale;
		}
	}
}

void VoiceAllocator::RenderUnison(float* left, float* right, uint32_t frames, const float* /*params*/) {
	bool any_active = false;
	for (uint8_t i = 0; i < max_voices_; i++) {
		if (voices_[i].active || voices_[i].env_amp.IsActive()) {
			any_active = true;
			break;
		}
	}

	if (!any_active) {
		drupiter::neon::ClearStereoBuffers(left, right, frames);
		return;
	}

	for (uint32_t i = 0; i < frames; i++) {
		unison_osc_.Process(&left[i], &right[i]);
	}
}

Voice* VoiceAllocator::AllocateVoice() {
	for (uint8_t i = 0; i < max_voices_; i++) {
		if (!voices_[i].active || !voices_[i].env_amp.IsActive()) {
			return &voices_[i];
		}
	}
	return StealVoice();
}

Voice* VoiceAllocator::StealVoice() {
	switch (allocation_strategy_) {
		case ALLOC_OLDEST_NOTE:
			return StealOldestVoice();
		case ALLOC_ROUND_ROBIN:
			return StealRoundRobinVoice();
		case ALLOC_FIRST_AVAILABLE:
		default:
			return &voices_[0];
	}
}

Voice* VoiceAllocator::StealOldestVoice() {
	uint8_t oldest_idx = 0;
	uint32_t oldest_time = voices_[0].note_on_time;
	for (uint8_t i = 1; i < max_voices_; i++) {
		if (voices_[i].note_on_time < oldest_time) {
			oldest_time = voices_[i].note_on_time;
			oldest_idx = i;
		}
	}
	return &voices_[oldest_idx];
}

Voice* VoiceAllocator::StealRoundRobinVoice() {
	round_robin_index_ = (round_robin_index_ + 1) % max_voices_;
	return &voices_[round_robin_index_];
}

void VoiceAllocator::TriggerVoice(Voice* voice, uint8_t note, uint8_t velocity, bool allow_legato) {
	float target_hz = common::MidiHelper::NoteToFreq(note);
	bool voice_still_sounding = voice->active;
	bool is_legato = allow_legato && voice_still_sounding;

	if (portamento_time_ms_ > 0.01f && is_legato && voice->pitch_hz > 0.0f) {
		voice->glide_target_hz = target_hz;
		voice->is_gliding = true;
		float freq_ratio = target_hz / voice->pitch_hz;
		float log_ratio = logf(freq_ratio);
		float portamento_time_sec = portamento_time_ms_ / 1000.0f;
		voice->glide_increment = log_ratio / (portamento_time_sec * sample_rate_);
	} else {
		voice->pitch_hz = target_hz;
		voice->glide_target_hz = target_hz;
		voice->is_gliding = false;
		voice->glide_increment = 0.0f;
	}

	voice->active = true;
	voice->midi_note = note;
	voice->velocity = common::MidiHelper::VelocityToFloat(velocity);
	voice->note_on_time = timestamp_;

	if (!is_legato) {
		voice->env_amp.Reset();
		voice->env_filter.Reset();
		voice->env_pitch.Reset();
		voice->env_amp.NoteOn();
		voice->env_filter.NoteOn();
		voice->env_pitch.NoteOn();
	}
}

void VoiceAllocator::AddActiveVoice(uint8_t voice_idx) {
	for (uint8_t i = 0; i < num_active_voices_; i++) {
		if (active_voice_list_[i] == voice_idx) {
			return;
		}
	}
	if (num_active_voices_ < max_voices_) {
		active_voice_list_[num_active_voices_++] = voice_idx;
	}
}

void VoiceAllocator::RemoveActiveVoice(uint8_t voice_idx) {
	uint8_t new_count = 0;
	for (uint8_t i = 0; i < num_active_voices_; i++) {
		if (active_voice_list_[i] != voice_idx) {
			active_voice_list_[new_count++] = active_voice_list_[i];
		}
	}
	num_active_voices_ = new_count;
}

void VoiceAllocator::UpdateActiveVoiceList() {
	uint8_t new_count = 0;
	for (uint8_t i = 0; i < num_active_voices_; i++) {
		uint8_t v_idx = active_voice_list_[i];
		Voice& voice = voices_[v_idx];
		if (voice.active || voice.env_amp.IsActive()) {
			active_voice_list_[new_count++] = v_idx;
		}
	}
	num_active_voices_ = new_count;
}

void VoiceAllocator::MarkVoiceInactive(uint8_t idx) {
	if (idx < max_voices_) {
		voices_[idx].active = false;
		core_.SetVoiceActive(idx, false);
	}
}

}  // namespace dsp
