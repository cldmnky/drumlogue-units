/*
 * File: voice_allocator.h
 * Project: Drupiter-Synth Hoover Synthesis Implementation
 * 
 * Description: Drupiter-specific voice allocator using common core.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include "jupiter_dco.h"
#include "jupiter_vcf.h"
#include "jupiter_env.h"
#include "unison_oscillator.h"
#include "../../common/voice_allocator.h"
#include "../../common/midi_helper.h"

#ifndef UNISON_VOICES
#define UNISON_VOICES 4
#endif

#ifndef DRUPITER_MAX_VOICES
#define DRUPITER_MAX_VOICES 4
#endif

namespace dsp {

// Synthesis modes (mapped to common::VoiceMode)
enum SynthMode {
	SYNTH_MODE_MONOPHONIC = 0,
	SYNTH_MODE_POLYPHONIC = 1,
	SYNTH_MODE_UNISON = 2
};

// Voice allocation strategies for polyphonic mode
enum VoiceAllocationStrategy {
	ALLOC_ROUND_ROBIN = 0,
	ALLOC_OLDEST_NOTE = 1,
	ALLOC_FIRST_AVAILABLE = 2
};

// Per-voice state structure
struct Voice {
	bool active;
	uint8_t midi_note;
	float velocity;
	float pitch_hz;
	uint32_t note_on_time;

	JupiterDCO dco1;
	JupiterDCO dco2;
	JupiterVCF vcf;
	JupiterEnvelope env_amp;
	JupiterEnvelope env_filter;
	JupiterEnvelope env_pitch;

	// Per-voice HPF state (Phase 2)
	float hpf_prev_output;
	float hpf_prev_input;

	float glide_target_hz;
	float glide_increment;
	bool is_gliding;

	Voice() : active(false), midi_note(0), velocity(0.0f),
			  pitch_hz(0.0f), note_on_time(0),
			  hpf_prev_output(0.0f), hpf_prev_input(0.0f),
			  glide_target_hz(0.0f), glide_increment(0.0f), is_gliding(false) {}

	void Init(float sample_rate);
	void Reset();
};

class VoiceAllocator {
public:
	VoiceAllocator();
	~VoiceAllocator();

	void Init(float sample_rate);
	void SetMode(SynthMode mode);
	void SetUnisonDetune(float detune_cents) {
		unison_detune_cents_ = detune_cents;
		unison_osc_.SetDetune(detune_cents);
	}

	void NoteOn(uint8_t note, uint8_t velocity);
	void NoteOff(uint8_t note);
	void AllNotesOff();

	// Rendering placeholders
	void RenderMonophonic(float* left, float* right, uint32_t frames,
						 const float* params);
	void RenderPolyphonic(float* left, float* right, uint32_t frames,
						 const float* params);
	void RenderUnison(float* left, float* right, uint32_t frames,
					 const float* params);

	inline void Render(float* left, float* right, uint32_t frames,
					  const float* params) {
		switch (mode_) {
			case SYNTH_MODE_MONOPHONIC:
				RenderMonophonic(left, right, frames, params);
				break;
			case SYNTH_MODE_POLYPHONIC:
				RenderPolyphonic(left, right, frames, params);
				break;
			case SYNTH_MODE_UNISON:
				RenderUnison(left, right, frames, params);
				break;
		}
	}

	bool IsAnyVoiceActive() const { return active_voices_ > 0; }
	const Voice& GetVoice(uint8_t idx) const { return voices_[idx]; }
	Voice& GetVoiceMutable(uint8_t idx) { return voices_[idx]; }
	SynthMode GetMode() const { return mode_; }
	UnisonOscillator& GetUnisonOscillator() { return unison_osc_; }
	bool HasHeldNotes() const { return core_.HasHeldNotes(); }

	void SetPortamentoTime(float time_ms) { portamento_time_ms_ = time_ms; }
	float GetPortamentoTime() const { return portamento_time_ms_; }

	void SetAllocationStrategy(VoiceAllocationStrategy strategy);
	void MarkVoiceInactive(uint8_t idx);

private:
	Voice voices_[DRUPITER_MAX_VOICES];
	uint8_t max_voices_;
	uint8_t active_voices_;
	uint8_t round_robin_index_;
	uint32_t timestamp_;

	uint8_t active_voice_list_[DRUPITER_MAX_VOICES];
	uint8_t num_active_voices_;

	SynthMode mode_;
	VoiceAllocationStrategy allocation_strategy_;

	UnisonOscillator unison_osc_;
	float unison_detune_cents_;
	float portamento_time_ms_;
	float sample_rate_;

	common::VoiceAllocatorCore core_;

	Voice* AllocateVoice();
	Voice* StealVoice();
	Voice* StealOldestVoice();
	Voice* StealRoundRobinVoice();
	void TriggerVoice(Voice* voice, uint8_t note, uint8_t velocity, bool allow_legato);

	void AddActiveVoice(uint8_t voice_idx);
	void RemoveActiveVoice(uint8_t voice_idx);
	void UpdateActiveVoiceList();

	VoiceAllocator(const VoiceAllocator&) = delete;
	VoiceAllocator& operator=(const VoiceAllocator&) = delete;
};

}  // namespace dsp
