# Bristol Jupiter-8 to Drumlogue Drupiter - Port Plan

**Repository**: https://github.com/nomadbyte/bristol-fixes  
**Target Platform**: Korg drumlogue (ARM Cortex-M7, 48kHz)  
**License Compatibility**: Bristol (GPL/MIT) → drumlogue unit (MIT compatible)

---

## Executive Summary

This document outlines the comprehensive port of the Bristol Jupiter-8 emulation to the Korg drumlogue as "Drupiter". The Jupiter-8 is a complex dual-layer polyphonic synthesizer with rich modulation capabilities, requiring careful architectural translation from Bristol's desktop engine to drumlogue's embedded real-time constraints.

**Key Challenges**:
- Bristol runs at variable sample rates (typically 44.1kHz); drumlogue is fixed at 48kHz
- Bristol uses dynamic voice allocation; drumlogue has monophonic+arpeggiator model
- Bristol has ~12 operators per voice; drumlogue has strict CPU/memory limits
- Desktop floating-point vs. embedded ARM optimization requirements

---

## 1. Architecture Analysis

### 1.1 Bristol Jupiter-8 Architecture

**Source Files** (from `bristol-fixes`):
```
bristol/
├── bristoljupiter.c        # Main voice initialization and control
├── bristoljupiter.h        # Voice structure definitions
├── junodco.c              # DCO (similar architecture to Jupiter)
├── envelope.c             # ADSR envelope generator
├── filter.c / filter2.c   # Various filter implementations
├── lfo.c                  # LFO oscillator
└── [various operators]    # Noise, amp, mix, etc.

brighton/
└── brightonJupiter.c      # GUI and parameter dispatch (reference only)
```

**Voice Structure** (from `bristoljupiter.c`):
```c
typedef struct bMods {
    unsigned int flags;        // Control flags (sync, mod routing, etc.)
    float *lout, *rout;       // Stereo outputs
    int voicecount;
    float lpan, rpan, gain;
    float lmix, rmix;
    float xmod;               // Cross-modulation depth
    float noisegain;
    float dcoLFO1FM;          // LFO→DCO modulation
    float dcoEnv1FM;          // Envelope→DCO modulation
    float pw;                 // Pulse width
    // ... additional mod routing
    float *dco2buf[BRISTOL_VOICECOUNT];  // Per-voice DCO2 buffer
} bmods;
```

**Operator Chain** (12 operators per voice):
```c
// From bristolJupiterInit():
initSoundAlgo(16, 0, ...)  // LFO-1
initSoundAlgo(16, 1, ...)  // LFO-2
initSoundAlgo( 1, 2, ...)  // Envelope-1 (Filter)
initSoundAlgo( 1, 3, ...)  // Envelope-2 (not used in basic config)
initSoundAlgo(30, 4, ...)  // DCO-1 (Bitone oscillator)
initSoundAlgo(30, 5, ...)  // DCO-2 (Bitone oscillator)
initSoundAlgo( 3, 6, ...)  // VCF (Filter)
initSoundAlgo( 1, 7, ...)  // VCF Envelope
initSoundAlgo( 2, 8, ...)  // VCA (Amplifier)
initSoundAlgo( 1, 9, ...)  // VCA Envelope
initSoundAlgo( 4, 10, ...) // Noise
initSoundAlgo(10, 11, ...) // HPF (High-pass filter)
```

### 1.2 Drumlogue SDK Architecture

**Target Structure**:
```cpp
drumlogue/drupiter/
├── Makefile               # Build configuration
├── config.mk             # Project configuration
├── header.c              # Unit metadata (developer ID, params)
├── unit.cc               # SDK callback implementations
├── drupiter_synth.h      # Main synth class
├── drupiter_synth.cc     # Implementation
└── dsp/                  # DSP components
    ├── jupiter_dco.h     # Dual DCO with sync/xmod
    ├── jupiter_dco.cc
    ├── jupiter_vcf.h     # Multi-mode filter
    ├── jupiter_vcf.cc
    ├── jupiter_lfo.h     # Dual LFO
    ├── jupiter_lfo.cc
    ├── jupiter_env.h     # ADSR envelopes
    ├── jupiter_env.cc
    └── common/           # Shared utilities
        ├── dsp_utils.h   # From drumlogue/common
        └── ...
```

**Callback Requirements**:
```cpp
// From logue-sdk unit API
unit_init()              // Initialize DSP state
unit_render()            // Process audio (48kHz float buffers)
unit_set_param_value()   // Handle parameter changes
unit_note_on()           // MIDI note on
unit_note_off()          // MIDI note off
unit_get_param_str()     // Parameter display strings
```

---

## 2. Component-by-Component Port Strategy

### 2.1 Digital Controlled Oscillators (DCO)

**Bristol Implementation** (`bristol/junodco.c` as reference):
- **Algorithm**: Phase accumulator with bitone wave generation
- **Waveforms**: Triangle, Ramp, Pulse, Square (mixable)
- **Features**: Sync, FM cross-modulation, PWM
- **Sample Rate**: Variable (44.1kHz typical)

**Port Strategy**:
```cpp
// dsp/jupiter_dco.h
class JupiterDCO {
public:
    enum Waveform { RAMP, SQUARE, PULSE, TRIANGLE };
    
    void Init(float sample_rate);
    void SetFrequency(float freq_hz);
    void SetPulseWidth(float pw);  // 0.0-1.0
    void SetSync(bool enabled);
    void SetWaveformMix(float ramp, float square);
    float Process();               // Single sample output
    
private:
    float phase_;                  // 0.0-1.0
    float phase_inc_;
    float pw_;
    float sync_phase_reset_;       // Sync from DCO1→DCO2
    
    // Wavetable for efficiency (vs. runtime calculation)
    static constexpr int kWavetableSize = 2048;
    float ramp_table_[kWavetableSize];
    float square_table_[kWavetableSize];
    
    void BuildWavetables();        // Called in Init()
};
```

**Key Differences**:
- **Bristol**: Uses `JUNODCO_WAVE_SZE` (2048) wavetable with linear interpolation
- **Drumlogue**: Fixed 48kHz, can use smaller tables or runtime generation
- **Optimization**: Pre-calculate common waveforms, use ARM NEON if needed

**Bristol Code Reference**:
```c
// From junodco.c operate():
wtp += ib[obp++] * transp;  // Phase accumulation
if (wtp >= JUNODCO_WAVE_SZE)
    wtp -= JUNODCO_WAVE_SZE;

// Waveform output with interpolation:
ob[obp] = wt[(int) wtp] + ((wt[(int) wtp + 1] - wt[(int) wtp]) * gdelta);
```

**Drumlogue Port**:
```cpp
float JupiterDCO::Process() {
    // Phase accumulation
    phase_ += phase_inc_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;
    
    // Wavetable lookup with linear interpolation
    float table_pos = phase_ * kWavetableSize;
    int index = static_cast<int>(table_pos);
    float frac = table_pos - index;
    
    float ramp = ramp_table_[index] + 
                 (ramp_table_[index + 1] - ramp_table_[index]) * frac;
    float square = square_table_[index] + 
                   (square_table_[index + 1] - square_table_[index]) * frac;
    
    // Mix waveforms (Bristol mixes, drumlogue can too)
    return ramp * ramp_mix_ + square * square_mix_;
}
```

### 2.2 Voltage Controlled Filter (VCF)

**Bristol Implementation** (`bristol/filter.c`, `bristol/filter2.c`):
- **Types**: Multi-mode (LP, BP, HP) and Huovilainen ladder
- **Algorithm**: Chamberlin state-variable filter (default)
- **Parameters**: Cutoff, Resonance, Keyboard tracking
- **Envelope Modulation**: Positive/negative from ENV-1 or ENV-2

**Bristol Filter Structure**:
```c
typedef struct bristolFILTERlocal {
    float velocity;
    float history[1024];
    float cutoff;
    float a[8], adash[8];
    float delay1, delay2, delay3, delay4, delay5;  // State variables
    float out1, out2, out3, out4;
    float az1, az2, az3, az4, az5;
    float ay1, ay2, ay3, ay4;
    float amf;
} bristolFILTERlocal;
```

**Port Strategy**:
```cpp
// dsp/jupiter_vcf.h
class JupiterVCF {
public:
    enum Mode { LP12, LP24, HP12, BP12 };
    
    void Init(float sample_rate);
    void SetCutoff(float freq_hz);          // 20Hz - 20kHz
    void SetResonance(float res);           // 0.0-1.0
    void SetKeyboardTracking(float amount); // 0.0-1.0
    void SetMode(Mode mode);
    float Process(float in);
    
private:
    float sample_rate_;
    float cutoff_hz_;
    float resonance_;
    Mode mode_;
    
    // Chamberlin state-variable filter states
    float lp_, bp_, hp_;  // Filter outputs
    float f_, q_;         // Coefficients
    
    void UpdateCoefficients();
};
```

**Bristol Code Reference**:
```c
// From filter.c operate() - Chamberlin algorithm:
lp = lp + f * bp;
hp = sample - lp - q * bp;
bp = f * hp + bp;

// Multi-mode output selection:
switch (mode) {
    case LP12: output = lp; break;
    case LP24: output = lp * lp; break;  // Cascade for 24dB/oct
    case HP12: output = hp; break;
    case BP12: output = bp; break;
}
```

**Drumlogue Port**:
```cpp
float JupiterVCF::Process(float in) {
    // Chamberlin state-variable filter
    lp_ = lp_ + f_ * bp_;
    hp_ = in - lp_ - q_ * bp_;
    bp_ = f_ * hp_ + bp_;
    
    // Mode selection
    switch (mode_) {
        case LP12: return lp_;
        case LP24: return lp_ * lp_;  // Approx 24dB/oct
        case HP12: return hp_;
        case BP12: return bp_;
    }
}

void JupiterVCF::UpdateCoefficients() {
    // From Bristol filter.c:
    f_ = 2.0f * sinf(M_PI * cutoff_hz_ / sample_rate_);
    q_ = 1.0f - resonance_;  // Simplified, adjust for Jupiter character
}
```

### 2.3 LFO (Low Frequency Oscillator)

**Bristol Implementation** (`bristol/lfo.c`):
- **Waveforms**: Triangle, Ramp, Square, Sample & Hold
- **Rate**: 0.1 - 50 Hz
- **Delay**: Up to 10 seconds (controlled by envelope)
- **Destinations**: VCO frequency, PWM, VCF cutoff

**Port Strategy**:
```cpp
// dsp/jupiter_lfo.h
class JupiterLFO {
public:
    enum Waveform { TRIANGLE, RAMP, SQUARE, SAMPLE_HOLD };
    
    void Init(float sample_rate);
    void SetFrequency(float freq_hz);
    void SetWaveform(Waveform wave);
    void SetDelay(float delay_sec);  // Ramp-up from 0
    void Trigger();                  // Reset delay on note-on
    float Process();                 // Returns -1.0 to +1.0
    
private:
    float phase_;
    float phase_inc_;
    Waveform waveform_;
    float delay_phase_;   // 0.0-1.0 for delay ramp
    float delay_time_;
    float sh_value_;      // Sample & hold current value
    
    float GenerateWaveform();
};
```

**Bristol Code Reference**:
```c
// From lfo.c:
// Triangle wave generation:
if (phase < 0.5f)
    output = phase * 4.0f - 1.0f;  // -1 to +1
else
    output = 3.0f - phase * 4.0f;  // +1 to -1

// Delay envelope:
if (delay_phase < 1.0f) {
    delay_phase += delay_inc;
    output *= delay_phase;  // Fade in LFO
}
```

**Drumlogue Port**:
```cpp
float JupiterLFO::Process() {
    phase_ += phase_inc_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;
    
    float output = GenerateWaveform();
    
    // Apply delay envelope
    if (delay_phase_ < 1.0f) {
        delay_phase_ += delay_inc_;
        output *= delay_phase_;
    }
    
    return output;
}

float JupiterLFO::GenerateWaveform() {
    switch (waveform_) {
        case TRIANGLE:
            return (phase_ < 0.5f) ? 
                (phase_ * 4.0f - 1.0f) : (3.0f - phase_ * 4.0f);
        case RAMP:
            return phase_ * 2.0f - 1.0f;
        case SQUARE:
            return (phase_ < 0.5f) ? -1.0f : 1.0f;
        case SAMPLE_HOLD:
            // Update S&H value at phase wrap
            if (phase_ < phase_inc_) {
                sh_value_ = (rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            }
            return sh_value_;
    }
}
```

### 2.4 ADSR Envelope Generators

**Bristol Implementation** (`bristol/envelope.c`):
- **Stages**: Attack, Decay, Sustain, Release
- **Curves**: Exponential (typical analog behavior)
- **Velocity Sensitivity**: Optional per envelope
- **Retrigger Modes**: Single, multi-trigger

**Bristol Envelope Structure**:
```c
typedef struct bristolENVlocal {
    float cgain;      // Current gain
    float egain;      // Target gain
    int state;        // Attack/Decay/Sustain/Release
} bristolENVlocal;
```

**Port Strategy**:
```cpp
// dsp/jupiter_env.h
class JupiterEnvelope {
public:
    enum State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    
    void Init(float sample_rate);
    void SetAttack(float time_sec);
    void SetDecay(float time_sec);
    void SetSustain(float level);    // 0.0-1.0
    void SetRelease(float time_sec);
    void NoteOn(float velocity);
    void NoteOff();
    float Process();                 // Returns current envelope value
    
private:
    State state_;
    float current_level_;
    float target_level_;
    float rate_;                     // Increment per sample
    float sample_rate_;
    
    // Precalculated rates
    float attack_rate_;
    float decay_rate_;
    float release_rate_;
    float sustain_level_;
    
    void UpdateRates();
};
```

**Bristol Code Reference**:
```c
// From envelope.c operate():
switch (state) {
    case ATTACK:
        cgain += attack;
        if (cgain >= 1.0f) {
            cgain = 1.0f;
            state = DECAY;
        }
        break;
    case DECAY:
        cgain -= decay;
        if (cgain <= sustain) {
            cgain = sustain;
            state = SUSTAIN;
        }
        break;
    case SUSTAIN:
        cgain = sustain;  // Hold
        break;
    case RELEASE:
        cgain -= release;
        if (cgain <= 0.0f) {
            cgain = 0.0f;
            state = IDLE;
        }
        break;
}
```

**Drumlogue Port**:
```cpp
float JupiterEnvelope::Process() {
    switch (state_) {
        case ATTACK:
            current_level_ += attack_rate_;
            if (current_level_ >= 1.0f) {
                current_level_ = 1.0f;
                state_ = DECAY;
            }
            break;
        
        case DECAY:
            current_level_ -= decay_rate_;
            if (current_level_ <= sustain_level_) {
                current_level_ = sustain_level_;
                state_ = SUSTAIN;
            }
            break;
        
        case SUSTAIN:
            current_level_ = sustain_level_;
            break;
        
        case RELEASE:
            current_level_ -= release_rate_;
            if (current_level_ <= 0.0f) {
                current_level_ = 0.0f;
                state_ = IDLE;
            }
            break;
        
        case IDLE:
            current_level_ = 0.0f;
            break;
    }
    
    return current_level_;
}

void JupiterEnvelope::UpdateRates() {
    // Convert time to rate (samples per increment)
    attack_rate_ = 1.0f / (attack_time_ * sample_rate_);
    decay_rate_ = 1.0f / (decay_time_ * sample_rate_);
    release_rate_ = 1.0f / (release_time_ * sample_rate_);
}
```

---

## 3. Main Synth Class Integration

**Class Structure**:
```cpp
// drupiter_synth.h
class DrupiterSynth {
public:
    static constexpr int kMaxPolyphony = 1;  // Monophonic
    
    struct Preset {
        // DCO-1
        uint8_t dco1_octave;
        uint8_t dco1_waveform;
        uint8_t dco1_pw;
        
        // DCO-2
        uint8_t dco2_octave;
        uint8_t dco2_detune;
        uint8_t dco2_waveform;
        uint8_t dco2_pw;
        
        // Mixer
        uint8_t dco1_level;
        uint8_t dco2_level;
        uint8_t noise_level;
        
        // VCF
        uint8_t vcf_cutoff;
        uint8_t vcf_resonance;
        uint8_t vcf_env_amount;
        uint8_t vcf_kbd_track;
        uint8_t vcf_mode;
        
        // Envelopes
        uint8_t vcf_attack;
        uint8_t vcf_decay;
        uint8_t vcf_sustain;
        uint8_t vcf_release;
        
        uint8_t vca_attack;
        uint8_t vca_decay;
        uint8_t vca_sustain;
        uint8_t vca_release;
        
        // LFO
        uint8_t lfo_rate;
        uint8_t lfo_delay;
        uint8_t lfo_waveform;
        uint8_t lfo_depth_vco;
        uint8_t lfo_depth_vcf;
        
        // Modulation
        uint8_t xmod_depth;     // DCO2 → DCO1 FM
        uint8_t sync_enable;    // DCO1 → DCO2 sync
        
        char name[14];          // 13 chars + null
    };
    
    int8_t Init(const unit_runtime_desc_t* desc);
    void Teardown();
    void Reset();
    void Resume();
    void Suspend();
    
    void Render(float* out, uint32_t frames);
    void SetParameter(uint8_t id, int32_t value);
    int32_t GetParameter(uint8_t id);
    const char* GetParameterStr(uint8_t id, int32_t value);
    
    void NoteOn(uint8_t note, uint8_t velocity);
    void NoteOff(uint8_t note);
    void AllNoteOff();
    
    void LoadPreset(uint8_t preset_id);
    void SavePreset(uint8_t preset_id);
    
private:
    // DSP components
    JupiterDCO dco1_, dco2_;
    JupiterVCF vcf_;
    JupiterLFO lfo_;
    JupiterEnvelope env_vcf_, env_vca_;
    
    // State
    bool gate_;
    uint8_t current_note_;
    uint8_t current_velocity_;
    float current_freq_hz_;
    
    // Internal buffers
    float dco1_out_;
    float dco2_out_;
    float noise_out_;
    float mixed_;
    float filtered_;
    float vcf_env_;
    float vca_env_;
    float lfo_out_;
    
    // Parameters
    Preset current_preset_;
    
    // Helper methods
    float NoteToFrequency(uint8_t note);
    void UpdateModulation();
    float ProcessNoise();  // Simple white noise generator
};
```

**Main Render Loop** (from Bristol `operateOneJupiter`):
```cpp
void DrupiterSynth::Render(float* out, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        // Process LFO (once per sample)
        lfo_out_ = lfo_.Process();
        
        // Process envelopes
        vcf_env_ = env_vcf_.Process();
        vca_env_ = env_vca_.Process();
        
        // Apply LFO modulation to DCO frequencies
        float freq1 = current_freq_hz_;
        float freq2 = current_freq_hz_ * dco2_detune_;
        
        if (current_preset_.lfo_depth_vco > 0) {
            float lfo_mod = lfo_out_ * (current_preset_.lfo_depth_vco / 127.0f);
            freq1 *= (1.0f + lfo_mod * 0.05f);  // ±5% vibrato
            freq2 *= (1.0f + lfo_mod * 0.05f);
        }
        
        dco1_.SetFrequency(freq1);
        dco2_.SetFrequency(freq2);
        
        // Cross-modulation: DCO2 → DCO1 (if enabled)
        if (current_preset_.xmod_depth > 0) {
            float xmod = dco2_out_ * (current_preset_.xmod_depth / 127.0f);
            // Apply FM to DCO1
            dco1_.ApplyFM(xmod);
        }
        
        // Process oscillators
        dco1_out_ = dco1_.Process();
        dco2_out_ = dco2_.Process();
        
        // Mix oscillators + noise
        mixed_ = dco1_out_ * (current_preset_.dco1_level / 127.0f)
               + dco2_out_ * (current_preset_.dco2_level / 127.0f)
               + ProcessNoise() * (current_preset_.noise_level / 127.0f);
        
        // Apply filter with envelope modulation
        float cutoff_mod = vcf_env_ * (current_preset_.vcf_env_amount / 64.0f - 1.0f);
        vcf_.SetCutoff(current_preset_.vcf_cutoff + cutoff_mod * 100.0f);
        filtered_ = vcf_.Process(mixed_);
        
        // Apply VCA with envelope
        float output = filtered_ * vca_env_;
        
        // Stereo output (mono source for now)
        out[i * 2] = output;
        out[i * 2 + 1] = output;
    }
}
```

---

## 4. Parameter Mapping

### 4.1 Header.c Parameter Definitions

```c
// header.c
const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api = UNIT_API_VERSION,
    .dev_id = 0x434C444DU,    // "CLDM"
    .unit_id = 0x00000004U,   // Unique ID for Drupiter
    .version = 0x010000U,     // v1.0.0
    .name = "Drupiter",       // Display name
    .num_presets = 6,
    .num_params = 24,         // 6 pages × 4 parameters
    .params = {
        // ==================== Page 1: DCO-1 ====================
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"D1 OCT"}},    // DCO-1 Octave
        {0, 127, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"D1 WAVE"}},  // Waveform
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"D1 PW"}},     // Pulse Width
        {0, 127, 100, 100, k_unit_param_type_none, 0, 0, 0, {"D1 LEVEL"}}, // Level
        
        // ==================== Page 2: DCO-2 ====================
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"D2 OCT"}},
        {0, 127, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"D2 WAVE"}},
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"D2 TUNE"}},   // Detune
        {0, 127, 100, 100, k_unit_param_type_none, 0, 0, 0, {"D2 LEVEL"}},
        
        // ==================== Page 3: VCF ====================
        {0, 127, 100, 100, k_unit_param_type_none, 0, 0, 0, {"CUTOFF"}},
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"RESO"}},
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"ENV AMT"}},   // Envelope amount
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"VCF TYP"}},    // Filter type
        
        // ==================== Page 4: VCF Envelope ====================
        {0, 127, 5, 5, k_unit_param_type_none, 0, 0, 0, {"F.ATK"}},
        {0, 127, 40, 40, k_unit_param_type_none, 0, 0, 0, {"F.DCY"}},
        {0, 127, 64, 64, k_unit_param_type_none, 0, 0, 0, {"F.SUS"}},
        {0, 127, 30, 30, k_unit_param_type_none, 0, 0, 0, {"F.REL"}},
        
        // ==================== Page 5: VCA Envelope ====================
        {0, 127, 1, 1, k_unit_param_type_none, 0, 0, 0, {"A.ATK"}},
        {0, 127, 50, 50, k_unit_param_type_none, 0, 0, 0, {"A.DCY"}},
        {0, 127, 100, 100, k_unit_param_type_none, 0, 0, 0, {"A.SUS"}},
        {0, 127, 20, 20, k_unit_param_type_none, 0, 0, 0, {"A.REL"}},
        
        // ==================== Page 6: LFO & Modulation ====================
        {0, 127, 40, 40, k_unit_param_type_none, 0, 0, 0, {"LFO RATE"}},
        {0, 3, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO WAV"}},    // Waveform
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"LFO>VCO"}},    // LFO to VCO
        {0, 127, 0, 0, k_unit_param_type_none, 0, 0, 0, {"XMOD"}},       // Cross-mod depth
    }
};
```

### 4.2 String Parameters

```cpp
const char* DrupiterSynth::GetParameterStr(uint8_t id, int32_t value) {
    static const char* waveforms[] = {"RAMP", "SQR", "PULSE", "TRI"};
    static const char* filter_types[] = {"LP12", "LP24", "HP12", "BP12"};
    static const char* lfo_waves[] = {"TRI", "RAMP", "SQR", "S&H"};
    
    switch (id) {
        case 1:  // DCO-1 Waveform
        case 5:  // DCO-2 Waveform
            return waveforms[value & 0x03];
        case 11: // VCF Type
            return filter_types[value & 0x03];
        case 17: // LFO Waveform
            return lfo_waves[value & 0x03];
        default:
            return nullptr;
    }
}
```

---

## 5. Optimization Strategies

### 5.1 CPU Budget Analysis

**Drumlogue Constraints**:
- ARM Cortex-M7 @ 216MHz
- Real-time audio @ 48kHz = 20.8μs per sample
- Target: <50% CPU for compatibility with other units

**Estimated Cycle Budgets**:
| Component | Complexity | Est. Cycles/Sample | Priority |
|-----------|------------|-------------------|----------|
| DCO-1     | Medium     | ~100              | High     |
| DCO-2     | Medium     | ~100              | High     |
| VCF       | High       | ~150              | High     |
| LFO       | Low        | ~30               | Medium   |
| ENV-VCF   | Low        | ~20               | High     |
| ENV-VCA   | Low        | ~20               | High     |
| Noise     | Low        | ~10               | Low      |
| Mix/Mod   | Low        | ~30               | High     |
| **Total** |            | **~460**          |          |

**Optimization Techniques**:
1. **Wavetable Pre-computation**: Store common waveforms in ROM
2. **NEON SIMD**: Process multiple samples in parallel where possible
3. **Fixed-Point Math**: Use Q31 format for envelope calculations
4. **Fast Math**: Approximate trig functions with polynomial

### 5.2 ARM-Specific Optimizations

```cpp
// Use ARM NEON for wavetable interpolation
#include <arm_neon.h>

void JupiterDCO::ProcessBlock(float* out, int count) {
    // Process 4 samples at once with NEON
    for (int i = 0; i < count; i += 4) {
        // Load phase increments
        float32x4_t phase_vec = vld1q_f32(&phases_[i]);
        // ... NEON processing
        vst1q_f32(&out[i], result_vec);
    }
}
```

### 5.3 Memory Optimization

**Bristol Memory Usage** (per voice):
- ~12 operators × ~200 bytes ≈ 2.4KB per voice
- Wavetables: ~8KB (shared)
- Total: ~10KB per voice

**Drumlogue Target**:
- Limit per-voice state to <2KB
- Share wavetables/lookup tables (ROM)
- Use stack allocation where possible

---

## 6. Testing Strategy

### 6.1 Unit Tests (Desktop)

```cpp
// test/test_jupiter_dco.cc
TEST(JupiterDCO, FrequencyAccuracy) {
    JupiterDCO dco;
    dco.Init(48000.0f);
    dco.SetFrequency(440.0f);  // A4
    
    // Process one cycle
    float samples[109]; // 48000/440 ≈ 109 samples
    for (int i = 0; i < 109; ++i) {
        samples[i] = dco.Process();
    }
    
    // Check phase wraps correctly
    // ... assertions
}

TEST(JupiterVCF, CutoffResponse) {
    JupiterVCF vcf;
    vcf.Init(48000.0f);
    vcf.SetCutoff(1000.0f);
    vcf.SetResonance(0.7f);
    
    // Impulse response test
    // ... assertions
}
```

### 6.2 Hardware Testing

**Test Sequence**:
1. Build `.drmlgunit` file
2. Load to drumlogue via USB
3. Test basic sound generation
4. Verify parameter response
5. Check CPU usage (via profiler)
6. Test with various MIDI inputs

**Known Issues to Watch**:
- **Undefined symbols**: Ensure all `static constexpr` members are defined (see `.github/copilot-instructions.md`)
- **CPU spikes**: Profile with different parameter settings
- **Denormal numbers**: Add DC offset or use `FTZ` mode

---

## 7. Implementation Phases

### Phase 1: Basic Monophonic Synth (Week 1-2)
- [ ] Create project structure (`config.mk`, `Makefile`, `header.c`)
- [ ] Implement single DCO with basic waveforms
- [ ] Implement basic Chamberlin VCF
- [ ] Implement ADSR envelopes
- [ ] Build and test `.drmlgunit` on hardware

### Phase 2: Dual DCO & Modulation (Week 3-4)
- [ ] Add second DCO with detune
- [ ] Implement DCO sync (DCO1 → DCO2)
- [ ] Implement cross-modulation (DCO2 → DCO1 FM)
- [ ] Add LFO with basic routing
- [ ] Test modulation routing

### Phase 3: Filter Modes & Optimization (Week 5-6)
- [ ] Implement multi-mode filter (LP12/LP24/HP12/BP12)
- [ ] Add keyboard tracking to filter
- [ ] Optimize DSP with ARM NEON
- [ ] Profile CPU usage
- [ ] Tune filter resonance for Jupiter character

### Phase 4: Polish & Presets (Week 7-8)
- [ ] Create 6 factory presets
- [ ] Implement preset save/load
- [ ] Add noise generator
- [ ] Fine-tune parameter ranges
- [ ] Documentation and release notes

---

## 8. Bristol Code Reference Mapping

### Key Bristol Files to Study

| Bristol File | Purpose | Drumlogue Equivalent |
|--------------|---------|---------------------|
| `bristol/bristoljupiter.c` | Voice init & control | `drupiter_synth.cc` |
| `bristol/junodco.c` | DCO implementation | `dsp/jupiter_dco.cc` |
| `bristol/filter.c` | Chamberlin filter | `dsp/jupiter_vcf.cc` |
| `bristol/envelope.c` | ADSR envelope | `dsp/jupiter_env.cc` |
| `bristol/lfo.c` | LFO generation | `dsp/jupiter_lfo.cc` |
| `brighton/brightonJupiter.c` | Parameter dispatch | `unit.cc` SetParameter |

### Critical Bristol Functions

```c
// From bristoljupiter.c
int bristolJupiterInit(audioMain *audiomain, Baudio *baudio);
int operateOneJupiter(audioMain *audiomain, Baudio *baudio, bristolVoice *voice, float *startbuf);

// From junodco.c
static int operate(bristolOP *operator, bristolVoice *voice, bristolOPParams *param, void *lcl);

// From filter.c
static int operate(bristolOP *operator, bristolVoice *voice, bristolOPParams *param, void *lcl);

// From envelope.c
static int operate(bristolOP *operator, bristolVoice *voice, bristolOPParams *param, void *lcl);
```

---

## 9. License & Attribution

**Bristol License**:
- GPL v3 (most Bristol code)
- MIT (some STM32 code)

**Drumlogue Unit License**:
- Must be compatible with drumlogue SDK (typically MIT/BSD)
- **Action**: Verify with Bristol maintainer for MIT-compatible licensing or clean-room implementation from specifications

**Attribution**:
```cpp
/*
 * Drupiter - Roland Jupiter-8 Emulation for Korg Drumlogue
 * Based on Bristol Jupiter-8 emulation (https://github.com/nomadbyte/bristol-fixes)
 * 
 * Original Bristol code:
 *   Copyright (c) by Nick Copeland <nickycopeland@hotmail.com> 1996,2012
 *   
 * Drumlogue port:
 *   Copyright (c) 2025 [Your Name]
 *   
 * Licensed under [LICENSE] - see LICENSE file for details
 */
```

---

## 10. Future Enhancements

**Post-v1.0 Features**:
- [ ] Dual-layer support (if drumlogue SDK allows multi-channel MIDI)
- [ ] Arpeggiator (using drumlogue's built-in capabilities)
- [ ] Additional filter types (Moog ladder, Oberheim)
- [ ] Chorus/Ensemble effect (Roland Dimension-D style)
- [ ] MIDI CC mapping for real-time control
- [ ] Unison mode (detuned voices for fat mono sound)

---

## 11. Resources & References

**Bristol Documentation**:
- https://github.com/nomadbyte/bristol-fixes
- Bristol Manual: `brighton/brightonreadme.h` (extensive emulator docs)

**Drumlogue SDK**:
- logue-SDK: `logue-sdk/platform/drumlogue/`
- Templates: `logue-sdk/platform/drumlogue/dummy-synth/`
- API Docs: `logue-sdk/README.md`

**Jupiter-8 References**:
- Service Manual: https://www.synthfool.com/docs/Roland/Jupiter_8/
- User Manual: Essential for understanding original parameter ranges
- Block Diagrams: Critical for signal flow understanding

**DSP References**:
- "Designing Sound" by Andy Farnell (DCO/VCF theory)
- "The Audio Programming Book" (filter implementations)
- ARM NEON Optimization: ARM Developer Documentation

---

## 12. Quick Start Checklist

1. **Setup Environment**:
   ```bash
   cd drumlogue/drupiter
   # Verify logue-sdk is in path
   ```

2. **Create Skeleton**:
   ```bash
   cp ../pepege-synth/Makefile .
   cp ../pepege-synth/config.mk .
   # Edit config.mk for drupiter
   ```

3. **Implement Minimal DSP**:
   - Start with single DCO
   - Add basic envelope
   - Test audio output

4. **Build & Test**:
   ```bash
   ../../build.sh drupiter
   # Copy drupiter.drmlgunit to drumlogue
   ```

5. **Iterate**:
   - Add components incrementally
   - Test on hardware frequently
   - Profile CPU usage

---

## Conclusion

This port represents a significant engineering effort to bring the rich Jupiter-8 sound to the embedded drumlogue platform. The key to success is:

1. **Incremental development**: Start with basic monophonic synth, add features systematically
2. **Frequent testing**: Validate on hardware early and often
3. **CPU awareness**: Profile continuously, optimize where needed
4. **Reference the original**: Bristol code is the gold standard for behavior

**Estimated Timeline**: 8-10 weeks for full feature set  
**Minimum Viable Product**: 2-3 weeks (basic dual DCO + filter + envelopes)

**Next Steps**:
1. Review this document with stakeholders
2. Setup development environment
3. Begin Phase 1 implementation
4. Schedule weekly progress reviews

---

**Document Version**: 1.0  
**Date**: December 10, 2025  
**Author**: GitHub Copilot (AI Assistant)  
**Status**: Planning - Ready for Implementation
