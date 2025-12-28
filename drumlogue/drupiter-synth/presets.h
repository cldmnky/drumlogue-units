/**
 * @file presets.h
 * @brief Factory presets for Drupiter Jupiter-8 synth
 *
 * Defines 12 factory presets with classic Jupiter-8 inspired sounds:
 * - Init: Basic starting point
 * - Bass: Punchy bass
 * - Lead: Sharp sync lead
 * - Pad: Warm detuned pad
 * - Brass: Bright brass with cross-modulation
 * - Strings: Lush detuned strings
 * - PWM Lead: Pulse width modulation example
 * - HPF Bass: High-pass filter example
 * - BP Lead: Bandpass filter example
 * - Poly Brass: Polyphonic mode example
 * - Chorus Pad: Effect usage example
 * - Square Vib: LFO waveform variety example
 */

#pragma once

#include "../common/preset_manager.h"
#include "drupiter_synth.h"
#include <cstring>

namespace presets {

// Preset structure compatible with PresetManager (24 params + hub values)
struct DrupiterPreset {
    char name[14];
    uint8_t params[DrupiterSynth::PARAM_COUNT];
    uint8_t hub_values[MOD_NUM_DESTINATIONS];
};

// Factory presets array
static const DrupiterPreset kFactoryPresets[12] = {
    // Preset 0: Init - Basic starting point for sound design
    // Simple dual-oscillator sawtooth in mono mode with clean filter
    // Purpose: Minimal patch to understand core Drupiter functionality
    {
        "Init 1",
        {
            // Page 1: DCO-1 (Oscillator 1)
            1,    // PARAM_DCO1_OCT: 8' (standard fundamental pitch)
            0,    // PARAM_DCO1_WAVE: SAW (sawtooth waveform - bright, harmonic-rich)
            50,   // PARAM_DCO1_PW: 50% (not used for SAW, but neutral)
            0,    // PARAM_XMOD: 0% (no cross-modulation oscillator->oscillator)
            
            // Page 2: DCO-2 (Oscillator 2)
            1,    // PARAM_DCO2_OCT: 8' (same octave as DCO1)
            0,    // PARAM_DCO2_WAVE: SAW (matches DCO1)
            50,   // PARAM_DCO2_TUNE: 50 (center = no detuning, perfect unison)
            0,    // PARAM_SYNC: OFF (no oscillator hard-sync)
            
            // Page 3: MIX & VCF (Filter settings)
            50,   // PARAM_OSC_MIX: 50% (equal blend of both oscillators)
            79,   // PARAM_VCF_CUTOFF: Open filter (bright sound)
            16,   // PARAM_VCF_RESONANCE: Low (minimal filter self-oscillation)
            50,   // PARAM_VCF_KEYFLW: 50% keyboard tracking (moderate pitch tracking)
            
            // Page 4: VCF Envelope (Filter modulation)
            4,    // PARAM_VCF_ATTACK: 4ms (quick filter response)
            31,   // PARAM_VCF_DECAY: Medium-fast decay to sustain
            50,   // PARAM_VCF_SUSTAIN: 50% (moderate sustain level)
            24,   // PARAM_VCF_RELEASE: Medium release time
            
            // Page 5: VCA Envelope (Amplitude modulation)
            1,    // PARAM_VCA_ATTACK: 1ms (almost instant note onset)
            39,   // PARAM_VCA_DECAY: Medium decay
            79,   // PARAM_VCA_SUSTAIN: High sustain (note stays open)
            16,   // PARAM_VCA_RELEASE: Short release (notes cut cleanly)
            
            // Page 6: LFO, MOD HUB & Effects
            32,   // PARAM_LFO_RATE: 32 (slow-to-moderate modulation rate)
            static_cast<uint8_t>(MOD_VCF_TYPE),  // PARAM_MOD_HUB: Currently editing VCF Type
            0,    // PARAM_MOD_AMT: 0% (no modulation, see hub_values below)
            0     // PARAM_EFFECT: 0 (chorus effect off)
        },
        {
            // Hub values: Modulation routing and synth mode parameters (18 total destinations)
            0,    // MOD_LFO_TO_PWM: 0% (no LFO modulation of oscillator pulse width)
            0,    // MOD_LFO_TO_VCF: 0% (no LFO modulation of filter cutoff)
            0,    // MOD_LFO_TO_VCO: 0% (no LFO modulation of oscillator pitch - no vibrato)
            0,    // MOD_ENV_TO_PWM: 0% (no envelope modulation of pulse width)
            0,    // MOD_ENV_TO_VCF: 0% (no envelope modulation of filter - static filter)
            0,    // MOD_HPF: 0 (high-pass filter off - keep all bass)
            1,    // MOD_VCF_TYPE: 1=LP24 (24dB/octave Moog-style lowpass filter)
            0,    // MOD_LFO_DELAY: 0 (LFO starts immediately, no delay)
            0,    // MOD_LFO_WAVE: 0=TRI (triangle wave LFO - smooth, symmetrical)
            0,    // MOD_LFO_ENV_AMT: 0% (LFO shape not modulated by ADSR envelope)
            100,  // MOD_VCA_LEVEL: 100% (volume at full output)
            0,    // MOD_VCA_LFO: 0% (no tremolo effect - volume stable)
            0,    // MOD_VCA_KYBD: 0% (velocity doesn't control output level)
            50,   // MOD_ENV_KYBD: 50% (envelope respects keyboard center pitch)
            0,    // MOD_SYNTH_MODE: 0=MONO (single voice, legato glide)
            10,   // MOD_UNISON_DETUNE: 10 cents (if switching to UNISON, minor detuning)
            50,   // MOD_ENV_TO_PITCH: 50 (center = envelope doesn't modulate pitch)
            0     // MOD_PORTAMENTO_TIME: 0ms (no glide between notes)
        }
    },
    
    // Preset 1: Bass - Punchy bass with articulate filter envelope
    // Deep, percussive bass with emphasis on filter dynamics and fast glide
    // Dual unison oscillators in low registers create powerful sub-bass
    {
        "Bass 1",
        {
            // DCO1: Narrow pulse wave for bright attack
            0,    // PARAM_DCO1_OCT: 16' (extra low octave, maximum depth)
            2,    // PARAM_DCO1_WAVE: PULSE (narrow/square wave - aggressive attack)
            31,   // PARAM_DCO1_PW: Very narrow pulse width (hollow character)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Sawtooth for fullness and harmonics
            0,    // PARAM_DCO2_OCT: 16' (same ultra-low pitch)
            0,    // PARAM_DCO2_WAVE: SAW (sawtooth provides fundamental and harmonics)
            50,   // PARAM_DCO2_TUNE: Center (no detune for unified sound)
            0,    // PARAM_SYNC: No sync between oscillators
            
            // Filter: Aggressive envelope-driven movement
            50,   // PARAM_OSC_MIX: 50% (equal blend of pulse and sawtooth)
            39,   // PARAM_VCF_CUTOFF: 39% (relatively closed, dark bass)
            39,   // PARAM_VCF_RESONANCE: High resonance (39% - emphasizes cutoff frequency)
            75,   // PARAM_VCF_KEYFLW: 75% keyboard tracking (filter opens on higher notes)
            
            // Filter envelope: Fast, punchy dynamics
            0,    // PARAM_VCF_ATTACK: 0ms (instant filter opening on note)
            27,   // PARAM_VCF_DECAY: Fast decay from peak
            16,   // PARAM_VCF_SUSTAIN: Low sustain (filter closes after attack)
            8,    // PARAM_VCF_RELEASE: Quick release (tight, controlled)
            
            // Amplitude envelope: Percussive note shape
            0,    // PARAM_VCA_ATTACK: 0ms (immediate volume onset)
            31,   // PARAM_VCA_DECAY: Medium decay
            63,   // PARAM_VCA_SUSTAIN: Medium sustain (note rings)
            12,   // PARAM_VCA_RELEASE: Short release (punchy ending)
            
            // Modulation
            32,   // PARAM_LFO_RATE: Moderate LFO rate
            static_cast<uint8_t>(MOD_VCF_TYPE),
            0,    // PARAM_MOD_AMT: 0% modulation
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Minimal LFO, focus on envelope dynamics
            0,    // MOD_LFO_TO_PWM: No PWM modulation
            0,    // MOD_LFO_TO_VCF: No filter LFO (filter envelope is main movement)
            0,    // MOD_LFO_TO_VCO: No pitch vibrato
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No secondary envelope modulation
            0,    // MOD_HPF: High-pass filter off (keep bass intact)
            1,    // MOD_VCF_TYPE: LP24 (Moog-style lowpass)
            0,    // MOD_LFO_DELAY: Immediate LFO
            0,    // MOD_LFO_WAVE: Triangle LFO shape
            0,    // MOD_LFO_ENV_AMT: LFO not shaped by envelope
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice for bass clarity)
            10,   // MOD_UNISON_DETUNE: Light detuning for richness
            50,   // MOD_ENV_TO_PITCH: No pitch envelope modulation
            15    // MOD_PORTAMENTO_TIME: 15ms glide (snappy, percussive bass)
        }
    },
    
    // Preset 2: Lead - Bright sync lead with dynamic filter modulation
    // Oscillator hard-sync creates rich, buzzy harmonics perfect for cutting leads
    // LFO modulates filter cutoff for evolving tonal character
    {
        "Lead 1",
        {
            // DCO1: Primary sawtooth oscillator
            1,    // PARAM_DCO1_OCT: 8' (standard lead octave)
            0,    // PARAM_DCO1_WAVE: SAW (harmonic-rich fundamentals)
            50,   // PARAM_DCO1_PW: 50% (N/A for sawtooth)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Synced oscillator for harmonics
            2,    // PARAM_DCO2_OCT: 4' (octave higher - creates brightness)
            0,    // PARAM_DCO2_WAVE: SAW
            50,   // PARAM_DCO2_TUNE: Center tune (locked to DCO1 pitch)
            2,    // PARAM_SYNC: HARD (DCO2 resets at DCO1 cycle - adds harmonics)
            
            // Filter: Bright, resonant character
            30,   // PARAM_OSC_MIX: 30% DCO2, 70% DCO1 (more DCO1 fundamental)
            71,   // PARAM_VCF_CUTOFF: 71% (bright, open filter)
            55,   // PARAM_VCF_RESONANCE: High resonance (55% - filter peak emphasized)
            40,   // PARAM_VCF_KEYFLW: 40% keyboard tracking (moderate pitch sensitivity)
            
            // Filter envelope: Musical, moderate dynamics
            4,    // PARAM_VCF_ATTACK: 4ms (slight envelope opening)
            24,   // PARAM_VCF_DECAY: Moderate decay
            47,   // PARAM_VCF_SUSTAIN: Medium-high sustain (note sits open)
            20,   // PARAM_VCF_RELEASE: Medium release
            
            // Amplitude envelope: Smooth note articulation
            2,    // PARAM_VCA_ATTACK: 2ms (very quick onset, nearly immediate)
            24,   // PARAM_VCA_DECAY: Moderate decay
            79,   // PARAM_VCA_SUSTAIN: High sustain (full note length)
            16,   // PARAM_VCA_RELEASE: Short release
            
            // Modulation
            50,   // PARAM_LFO_RATE: Moderate LFO for filter sweeps
            static_cast<uint8_t>(MOD_LFO_TO_VCF),  // Controlling filter cutoff
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: LFO drives filter for expressiveness
            0,    // MOD_LFO_TO_PWM: No PWM modulation
            30,   // MOD_LFO_TO_VCF: 30% (LFO sweeps filter cutoff - creates movement)
            0,    // MOD_LFO_TO_VCO: No pitch vibrato (stable lead)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No secondary envelope modulation
            0,    // MOD_HPF: High-pass filter off
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: Immediate LFO sweep
            0,    // MOD_LFO_WAVE: Triangle LFO (smooth modulation)
            0,    // MOD_LFO_ENV_AMT: LFO not shaped by envelope
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice for clear lead)
            10,   // MOD_UNISON_DETUNE: Minimal detuning
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            25    // MOD_PORTAMENTO_TIME: 25ms glide (responsive but smooth)
        }
    },
    
    // Preset 3: Pad - Lush, warm pad with rich detuning and gentle vibrato
    // Two slightly detuned sawtooths create natural chorus effect
    // Slow envelopes and subtle vibrato provide ethereal, evolving character
    {
        "Pad 1",
        {
            // DCO1: Foundation sawtooth
            1,    // PARAM_DCO1_OCT: 8' (standard pad register)
            0,    // PARAM_DCO1_WAVE: SAW (harmonic foundation)
            50,   // PARAM_DCO1_PW: 50% (not used for SAW)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Slightly detuned for richness
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW
            53,   // PARAM_DCO2_TUNE: 53% (slight detune - creates natural chorus)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Warm, smooth tone
            50,   // PARAM_OSC_MIX: 50% (equal blend of both oscillators)
            63,   // PARAM_VCF_CUTOFF: 63% (medium-bright, warm tone)
            20,   // PARAM_VCF_RESONANCE: Low (20% - smooth, no peaks)
            20,   // PARAM_VCF_KEYFLW: 20% keyboard tracking (minimal pitch tracking)
            
            // Filter envelope: Slow, smooth evolution
            35,   // PARAM_VCF_ATTACK: 35ms (gradual filter opening - warm swell)
            39,   // PARAM_VCF_DECAY: Moderate decay from peak
            55,   // PARAM_VCF_SUSTAIN: Medium sustain level
            39,   // PARAM_VCF_RELEASE: Moderate release (no abrupt cutoff)
            
            // Amplitude envelope: Slow swell and fade
            39,   // PARAM_VCA_ATTACK: 39ms (slow onset - pad swells in)
            39,   // PARAM_VCA_DECAY: Moderate decay
            79,   // PARAM_VCA_SUSTAIN: High sustain (note rings for full duration)
            55,   // PARAM_VCA_RELEASE: Long release (pad fades out slowly)
            
            // Modulation
            35,   // PARAM_LFO_RATE: Slow LFO (about 0.5-1 Hz - gentle modulation)
            static_cast<uint8_t>(MOD_LFO_TO_VCO),  // Controlling pitch for vibrato
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Gentle vibrato and detuning for richness
            0,    // MOD_LFO_TO_PWM: No PWM modulation
            0,    // MOD_LFO_TO_VCF: No filter modulation (clean, stable tone)
            15,   // MOD_LFO_TO_VCO: 15% (subtle pitch vibrato - warm shimmer)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off (preserve lows)
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: Immediate vibrato
            0,    // MOD_LFO_WAVE: Triangle LFO (smooth modulation)
            0,    // MOD_LFO_ENV_AMT: LFO not shaped
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Stable volume
            50,   // MOD_ENV_KYBD: Centered envelope
            0,    // MOD_SYNTH_MODE: MONO (single voice)
            15,   // MOD_UNISON_DETUNE: 15 cents (adds richness and width)
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            40    // MOD_PORTAMENTO_TIME: 40ms glide (smooth note transitions)
        }
    },
    
    // Preset 4: Brass - Bold brass with envelope-driven filter dynamics
    // Cross-modulation adds harmonic complexity and warmth
    // Filter envelope tracks amplitude envelope for unified brass character
    {
        "Brass 1",
        {
            // DCO1: Main sawtooth with cross-modulation
            1,    // PARAM_DCO1_OCT: 8' (brass register)
            0,    // PARAM_DCO1_WAVE: SAW
            50,   // PARAM_DCO1_PW: 50% (not used)
            15,   // PARAM_XMOD: 15% cross-modulation (adds harmonics and warmth)
            
            // DCO2: Supporting oscillator
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW
            50,   // PARAM_DCO2_TUNE: Center (locked in pitch)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Bright, focused
            40,   // PARAM_OSC_MIX: 40% DCO2, 60% DCO1 (emphasizes DCO1)
            59,   // PARAM_VCF_CUTOFF: 59% (bright, cutting tone)
            24,   // PARAM_VCF_RESONANCE: Moderate (24% - adds brightness without harshness)
            60,   // PARAM_VCF_KEYFLW: 60% keyboard tracking (filter tracks pitch for consistent brightness)
            
            // Filter envelope: Medium response time
            12,   // PARAM_VCF_ATTACK: 12ms (quick filter open, controlled swell)
            35,   // PARAM_VCF_DECAY: Moderate decay
            51,   // PARAM_VCF_SUSTAIN: Medium sustain
            27,   // PARAM_VCF_RELEASE: Medium release
            
            // Amplitude envelope: Characteristic brass shape
            12,   // PARAM_VCA_ATTACK: 12ms (quick onset, authentic brass articulation)
            35,   // PARAM_VCA_DECAY: Moderate decay
            71,   // PARAM_VCA_SUSTAIN: High sustain (note rings)
            24,   // PARAM_VCA_RELEASE: Medium release
            
            // Modulation
            40,   // PARAM_LFO_RATE: Moderate rate
            static_cast<uint8_t>(MOD_ENV_TO_VCF),  // Envelope modulates filter
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Envelope-driven filter, cross-oscillator richness
            0,    // MOD_LFO_TO_PWM: No PWM modulation
            0,    // MOD_LFO_TO_VCF: No LFO->filter (envelope controls filter)
            0,    // MOD_LFO_TO_VCO: No vibrato (stable pitch)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            40,   // MOD_ENV_TO_VCF: 40% (ADSR envelope modulates filter - dynamic brass)
            0,    // MOD_HPF: High-pass filter off
            1,    // MOD_VCF_TYPE: LP24 (Moog filter)
            0,    // MOD_LFO_DELAY: N/A (not using LFO)
            0,    // MOD_LFO_WAVE: Triangle (default)
            0,    // MOD_LFO_ENV_AMT: N/A
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Stable volume across register
            50,   // MOD_ENV_KYBD: Centered envelope
            0,    // MOD_SYNTH_MODE: MONO (single voice clarity)
            10,   // MOD_UNISON_DETUNE: 10 cents (light richness)
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            50    // MOD_PORTAMENTO_TIME: 50ms glide (smooth legato, authentic brass)
        }
    },
    
    // Preset 5: Strings - Rich string section with detuning and warm vibrato
    // Heavily detuned oscillators create lush, ensemble string effect
    // Warm vibrato and long envelopes emulate sustaining string pad
    {
        "String 1",
        {
            // DCO1: Foundation oscillator
            1,    // PARAM_DCO1_OCT: 8' (string register)
            0,    // PARAM_DCO1_WAVE: SAW (harmonic foundation)
            50,   // PARAM_DCO1_PW: 50% (not used)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Heavily detuned for ensemble effect
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW
            55,   // PARAM_DCO2_TUNE: 55% (significant detune - creates chorus/ensemble)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Warm, non-aggressive
            50,   // PARAM_OSC_MIX: 50% (equal blend - important for fullness)
            75,   // PARAM_VCF_CUTOFF: 75% (bright but warm, full harmonics)
            16,   // PARAM_VCF_RESONANCE: Very low (16% - smooth, no peaks)
            25,   // PARAM_VCF_KEYFLW: 25% keyboard tracking (minimal - consistent brightness)
            
            // Filter envelope: Long, smooth evolution
            47,   // PARAM_VCF_ATTACK: 47ms (slow filter swell - characteristic of strings)
            43,   // PARAM_VCF_DECAY: Moderate decay
            59,   // PARAM_VCF_SUSTAIN: Medium-high sustain
            47,   // PARAM_VCF_RELEASE: Long release (smooth fade)
            
            // Amplitude envelope: Slow, orchestral feel
            51,   // PARAM_VCA_ATTACK: 51ms (slow onset - swells in like bowed strings)
            43,   // PARAM_VCA_DECAY: Moderate decay
            79,   // PARAM_VCA_SUSTAIN: High sustain (full note duration)
            63,   // PARAM_VCA_RELEASE: Very long release (fades gracefully)
            
            // Modulation
            38,   // PARAM_LFO_RATE: Slow LFO (about 0.8 Hz - gentle modulation)
            static_cast<uint8_t>(MOD_LFO_TO_VCO),  // Controlling pitch for vibrato
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Warm vibrato and ensemble detuning
            0,    // MOD_LFO_TO_PWM: No PWM modulation
            0,    // MOD_LFO_TO_VCF: No filter modulation (clean, sustained tone)
            20,   // MOD_LFO_TO_VCO: 20% (gentle pitch vibrato - warm, expressive)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off (preserve bass resonance)
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: Immediate vibrato
            0,    // MOD_LFO_WAVE: Triangle LFO (smooth, orchestral vibrato)
            0,    // MOD_LFO_ENV_AMT: LFO not shaped
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Stable volume
            50,   // MOD_ENV_KYBD: Centered envelope
            0,    // MOD_SYNTH_MODE: MONO (single voice)
            20,   // MOD_UNISON_DETUNE: 20 cents (significant detuning for lush ensemble effect)
            50,   // MOD_ENV_TO_PITCH: No pitch envelope modulation
            60    // MOD_PORTAMENTO_TIME: 60ms glide (smooth, orchestral transitions)
        }
    },
    
    // Preset 6: PWM Lead - Pulse width modulation example
    // LFO modulates pulse width for rich, evolving timbral character
    // Classic Jupiter-8 PWM technique for creamy, animated leads
    {
        "PWM Lead",
        {
            // DCO1: Pulse wave for PWM modulation
            1,    // PARAM_DCO1_OCT: 8' (standard lead octave)
            2,    // PARAM_DCO1_WAVE: PULSE (square wave - PWM target)
            50,   // PARAM_DCO1_PW: 50% (center pulse width)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Supporting sawtooth
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW (harmonic foundation)
            50,   // PARAM_DCO2_TUNE: Center (no detune)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Bright, focused
            40,   // PARAM_OSC_MIX: 40% DCO2, 60% DCO1 (emphasize PWM oscillator)
            67,   // PARAM_VCF_CUTOFF: 67% (bright but not harsh)
            31,   // PARAM_VCF_RESONANCE: Moderate (31% - adds presence)
            45,   // PARAM_VCF_KEYFLW: 45% keyboard tracking (moderate)
            
            // Filter envelope: Moderate response
            8,    // PARAM_VCF_ATTACK: 8ms (quick filter opening)
            27,   // PARAM_VCF_DECAY: Moderate decay
            55,   // PARAM_VCF_SUSTAIN: Medium-high sustain
            24,   // PARAM_VCF_RELEASE: Medium release
            
            // Amplitude envelope: Smooth articulation
            4,    // PARAM_VCA_ATTACK: 4ms (quick onset)
            31,   // PARAM_VCA_DECAY: Moderate decay
            79,   // PARAM_VCA_SUSTAIN: High sustain (note rings)
            20,   // PARAM_VCA_RELEASE: Medium release
            
            // Modulation: LFO drives PWM
            45,   // PARAM_LFO_RATE: Moderate LFO (about 1Hz - noticeable PWM)
            static_cast<uint8_t>(MOD_LFO_TO_PWM),  // Controlling pulse width
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: LFO creates rich PWM modulation
            35,   // MOD_LFO_TO_PWM: 35% (noticeable pulse width modulation)
            0,    // MOD_LFO_TO_VCF: No filter LFO
            0,    // MOD_LFO_TO_VCO: No pitch vibrato (clean PWM focus)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM (LFO is main modulation)
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: Immediate PWM modulation
            0,    // MOD_LFO_WAVE: Triangle LFO (smooth PWM modulation)
            0,    // MOD_LFO_ENV_AMT: LFO not shaped
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice clarity)
            8,    // MOD_UNISON_DETUNE: Light detuning for richness
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            20    // MOD_PORTAMENTO_TIME: 20ms glide (smooth transitions)
        }
    },
    
    // Preset 7: HPF Bass - High-pass filter example
    // High-pass filter removes mud, adds clarity and punch
    // Modern bass sound with tight, focused low end
    {
        "HPF Bass",
        {
            // DCO1: Sawtooth for rich harmonics
            0,    // PARAM_DCO1_OCT: 16' (deep bass)
            0,    // PARAM_DCO1_WAVE: SAW (harmonic-rich)
            50,   // PARAM_DCO1_PW: 50% (not used)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Pulse for bite and attack
            0,    // PARAM_DCO2_OCT: 16' (same deep range)
            2,    // PARAM_DCO2_WAVE: PULSE (bright attack)
            50,   // PARAM_DCO2_TUNE: Center (no detune)
            0,    // PARAM_SYNC: No sync
            
            // Filter: High-pass removes mud
            50,   // PARAM_OSC_MIX: 50% (equal blend)
            55,   // PARAM_VCF_CUTOFF: 55% (moderate cutoff)
            20,   // PARAM_VCF_RESONANCE: Low (20% - clean sound)
            80,   // PARAM_VCF_KEYFLW: 80% keyboard tracking (filter follows pitch)
            
            // Filter envelope: Punchy dynamics
            0,    // PARAM_VCF_ATTACK: 0ms (instant filter response)
            20,   // PARAM_VCF_DECAY: Fast decay
            25,   // PARAM_VCF_SUSTAIN: Low sustain (filter closes)
            12,   // PARAM_VCF_RELEASE: Quick release
            
            // Amplitude envelope: Tight and punchy
            0,    // PARAM_VCA_ATTACK: 0ms (immediate onset)
            27,   // PARAM_VCA_DECAY: Fast decay
            67,   // PARAM_VCA_SUSTAIN: Medium-high sustain
            16,   // PARAM_VCA_RELEASE: Short release
            
            // Modulation: Focus on HPF
            32,   // PARAM_LFO_RATE: Moderate rate
            static_cast<uint8_t>(MOD_HPF),  // Controlling high-pass filter
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: High-pass filter adds clarity
            0,    // MOD_LFO_TO_PWM: No PWM
            0,    // MOD_LFO_TO_VCF: No filter LFO
            0,    // MOD_LFO_TO_VCO: No vibrato
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            45,   // MOD_HPF: 45% (removes mud, adds clarity and punch)
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: N/A
            0,    // MOD_LFO_WAVE: Triangle (default)
            0,    // MOD_LFO_ENV_AMT: N/A
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice for bass clarity)
            12,   // MOD_UNISON_DETUNE: Light detuning
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            12    // MOD_PORTAMENTO_TIME: 12ms glide (tight bass)
        }
    },
    
    // Preset 8: BP Lead - Bandpass filter example
    // Bandpass filter creates nasal, resonant character
    // Perfect for funky leads and vintage synth sounds
    {
        "BP Lead",
        {
            // DCO1: Sawtooth for harmonics
            1,    // PARAM_DCO1_OCT: 8' (lead register)
            0,    // PARAM_DCO1_WAVE: SAW (harmonic foundation)
            50,   // PARAM_DCO1_PW: 50% (not used)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Pulse for brightness
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            2,    // PARAM_DCO2_WAVE: PULSE (bright edge)
            50,   // PARAM_DCO2_TUNE: Center (no detune)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Bandpass for nasal character
            50,   // PARAM_OSC_MIX: 50% (equal blend)
            55,   // PARAM_VCF_CUTOFF: 55% (mid-range focus)
            67,   // PARAM_VCF_RESONANCE: High (67% - emphasizes bandpass peak)
            35,   // PARAM_VCF_KEYFLW: 35% keyboard tracking (moderate)
            
            // Filter envelope: Shapes the bandpass response
            8,    // PARAM_VCF_ATTACK: 8ms (quick filter response)
            31,   // PARAM_VCF_DECAY: Moderate decay
            47,   // PARAM_VCF_SUSTAIN: Medium sustain
            27,   // PARAM_VCF_RELEASE: Medium release
            
            // Amplitude envelope: Smooth articulation
            4,    // PARAM_VCA_ATTACK: 4ms (quick onset)
            27,   // PARAM_VCA_DECAY: Moderate decay
            79,   // PARAM_VCA_SUSTAIN: High sustain
            20,   // PARAM_VCA_RELEASE: Medium release
            
            // Modulation: Focus on filter type
            40,   // PARAM_LFO_RATE: Moderate rate
            static_cast<uint8_t>(MOD_VCF_TYPE),  // Controlling filter type
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Bandpass filter creates nasal character
            0,    // MOD_LFO_TO_PWM: No PWM
            0,    // MOD_LFO_TO_VCF: No filter LFO
            0,    // MOD_LFO_TO_VCO: No vibrato
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off
            2,    // MOD_VCF_TYPE: BP12 (bandpass filter - nasal character)
            0,    // MOD_LFO_DELAY: N/A
            0,    // MOD_LFO_WAVE: Triangle (default)
            0,    // MOD_LFO_ENV_AMT: N/A
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice)
            8,    // MOD_UNISON_DETUNE: Light detuning
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            25    // MOD_PORTAMENTO_TIME: 25ms glide (smooth transitions)
        }
    },
    
    // Preset 9: Poly Brass - Polyphonic mode example
    // Polyphonic brass with rich cross-modulation
    // Demonstrates multi-voice polyphony capabilities
    {
        "Poly Brass",
        {
            // DCO1: Sawtooth foundation
            1,    // PARAM_DCO1_OCT: 8' (brass register)
            0,    // PARAM_DCO1_WAVE: SAW (harmonic foundation)
            50,   // PARAM_DCO1_PW: 50% (not used)
            20,   // PARAM_XMOD: 20% cross-modulation (adds warmth)
            
            // DCO2: Supporting oscillator
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW
            50,   // PARAM_DCO2_TUNE: Center (no detune)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Bright brass character
            45,   // PARAM_OSC_MIX: 45% DCO2, 55% DCO1 (slight DCO1 emphasis)
            63,   // PARAM_VCF_CUTOFF: 63% (bright but musical)
            27,   // PARAM_VCF_RESONANCE: Moderate (27% - adds presence)
            55,   // PARAM_VCF_KEYFLW: 55% keyboard tracking (good brass response)
            
            // Filter envelope: Brass-like dynamics
            8,    // PARAM_VCF_ATTACK: 8ms (quick filter opening)
            31,   // PARAM_VCF_DECAY: Moderate decay
            51,   // PARAM_VCF_SUSTAIN: Medium sustain
            24,   // PARAM_VCF_RELEASE: Medium release
            
            // Amplitude envelope: Authentic brass articulation
            6,    // PARAM_VCA_ATTACK: 6ms (controlled onset)
            35,   // PARAM_VCA_DECAY: Moderate decay
            75,   // PARAM_VCA_SUSTAIN: High sustain (brass rings)
            27,   // PARAM_VCA_RELEASE: Medium release
            
            // Modulation: Cross-modulation focus
            36,   // PARAM_LFO_RATE: Moderate rate
            static_cast<uint8_t>(MOD_LFO_TO_VCO),  // Could control cross-mod if implemented
            0,    // PARAM_MOD_AMT: 0%
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Polyphonic brass with cross-modulation
            0,    // MOD_LFO_TO_PWM: No PWM
            0,    // MOD_LFO_TO_VCF: No filter LFO
            0,    // MOD_LFO_TO_VCO: No vibrato
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: N/A
            0,    // MOD_LFO_WAVE: Triangle (default)
            0,    // MOD_LFO_ENV_AMT: N/A
            95,   // MOD_VCA_LEVEL: 95% (slight headroom for polyphony)
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            1,    // MOD_SYNTH_MODE: POLY (4-voice polyphony)
            12,   // MOD_UNISON_DETUNE: Light detuning for richness
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            35    // MOD_PORTAMENTO_TIME: 35ms glide (smooth poly transitions)
        }
    },
    
    // Preset 10: Chorus Pad - Effect usage example
    // Chorus effect adds width and movement to warm pad
    // Demonstrates stereo chorus capabilities
    {
        "Chorus Pad",
        {
            // DCO1: Warm sawtooth
            1,    // PARAM_DCO1_OCT: 8' (pad register)
            0,    // PARAM_DCO1_WAVE: SAW (warm harmonics)
            50,   // PARAM_DCO1_PW: 50% (not used)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Slightly detuned for natural chorus
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW
            52,   // PARAM_DCO2_TUNE: 52% (slight detune - natural chorus)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Warm, smooth
            50,   // PARAM_OSC_MIX: 50% (equal blend)
            59,   // PARAM_VCF_CUTOFF: 59% (warm but present)
            16,   // PARAM_VCF_RESONANCE: Low (16% - smooth)
            25,   // PARAM_VCF_KEYFLW: 25% keyboard tracking (minimal)
            
            // Filter envelope: Slow evolution
            31,   // PARAM_VCF_ATTACK: 31ms (slow filter swell)
            39,   // PARAM_VCF_DECAY: Moderate decay
            55,   // PARAM_VCF_SUSTAIN: Medium sustain
            43,   // PARAM_VCF_RELEASE: Long release
            
            // Amplitude envelope: Slow, lush
            35,   // PARAM_VCA_ATTACK: 35ms (slow onset - pad swells in)
            43,   // PARAM_VCA_DECAY: Moderate decay
            79,   // PARAM_VCA_SUSTAIN: High sustain
            59,   // PARAM_VCA_RELEASE: Very long release
            
            // Modulation: Gentle vibrato
            28,   // PARAM_LFO_RATE: Slow LFO (gentle modulation)
            static_cast<uint8_t>(MOD_LFO_TO_VCO),  // Controlling pitch
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            1     // PARAM_EFFECT: CHORUS (adds width and movement)
        },
        {
            // Modulation: Chorus effect with gentle vibrato
            0,    // MOD_LFO_TO_PWM: No PWM
            0,    // MOD_LFO_TO_VCF: No filter LFO
            12,   // MOD_LFO_TO_VCO: 12% (gentle vibrato)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: Immediate vibrato
            0,    // MOD_LFO_WAVE: Triangle LFO (smooth modulation)
            0,    // MOD_LFO_ENV_AMT: LFO not shaped
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice)
            18,   // MOD_UNISON_DETUNE: 18 cents (chorus-like detuning)
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            45    // MOD_PORTAMENTO_TIME: 45ms glide (smooth transitions)
        }
    },
    
    // Preset 11: Square Vib - LFO waveform variety example
    // Square wave LFO creates distinctive stepped vibrato
    // Demonstrates different LFO waveforms beyond triangle
    {
        "Square Vib",
        {
            // DCO1: Clean sawtooth
            1,    // PARAM_DCO1_OCT: 8' (melodic register)
            0,    // PARAM_DCO1_WAVE: SAW (clean harmonics)
            50,   // PARAM_DCO1_PW: 50% (not used)
            0,    // PARAM_XMOD: No cross-modulation
            
            // DCO2: Supporting oscillator
            1,    // PARAM_DCO2_OCT: 8' (same octave)
            0,    // PARAM_DCO2_WAVE: SAW
            50,   // PARAM_DCO2_TUNE: Center (no detune)
            0,    // PARAM_SYNC: No sync
            
            // Filter: Clean and bright
            50,   // PARAM_OSC_MIX: 50% (equal blend)
            71,   // PARAM_VCF_CUTOFF: 71% (bright and clear)
            20,   // PARAM_VCF_RESONANCE: Low (20% - clean sound)
            40,   // PARAM_VCF_KEYFLW: 40% keyboard tracking (moderate)
            
            // Filter envelope: Moderate dynamics
            12,   // PARAM_VCF_ATTACK: 12ms (quick response)
            31,   // PARAM_VCF_DECAY: Moderate decay
            59,   // PARAM_VCF_SUSTAIN: Medium-high sustain
            27,   // PARAM_VCF_RELEASE: Medium release
            
            // Amplitude envelope: Smooth articulation
            8,    // PARAM_VCA_ATTACK: 8ms (quick onset)
            31,   // PARAM_VCA_DECAY: Moderate decay
            83,   // PARAM_VCA_SUSTAIN: Very high sustain
            24,   // PARAM_VCA_RELEASE: Medium release
            
            // Modulation: Square wave LFO
            55,   // PARAM_LFO_RATE: Faster LFO (more noticeable vibrato)
            static_cast<uint8_t>(MOD_LFO_WAVE),  // Controlling LFO waveform
            0,    // PARAM_MOD_AMT: 0% (amount in hub_values)
            0     // PARAM_EFFECT: Off
        },
        {
            // Modulation: Square wave creates stepped vibrato
            0,    // MOD_LFO_TO_PWM: No PWM
            0,    // MOD_LFO_TO_VCF: No filter LFO
            25,   // MOD_LFO_TO_VCO: 25% (noticeable pitch modulation)
            0,    // MOD_ENV_TO_PWM: No envelope->PWM
            0,    // MOD_ENV_TO_VCF: No envelope->filter
            0,    // MOD_HPF: High-pass filter off
            1,    // MOD_VCF_TYPE: LP24 (Moog-style filter)
            0,    // MOD_LFO_DELAY: Immediate vibrato
            2,    // MOD_LFO_WAVE: SQR (square wave - stepped vibrato)
            0,    // MOD_LFO_ENV_AMT: LFO not shaped
            100,  // MOD_VCA_LEVEL: Full volume
            0,    // MOD_VCA_LFO: No tremolo
            0,    // MOD_VCA_KYBD: Volume stable
            50,   // MOD_ENV_KYBD: Envelope centered
            0,    // MOD_SYNTH_MODE: MONO (single voice)
            10,   // MOD_UNISON_DETUNE: Light detuning
            50,   // MOD_ENV_TO_PITCH: No pitch envelope
            30    // MOD_PORTAMENTO_TIME: 30ms glide (smooth transitions)
        }
    }
};

}  // namespace presets
