/**
 *  @file elements_synth.h  
 *  @brief Modal synthesis synth for drumlogue
 *
 *  A modal synthesis implementation inspired by Mutable Instruments Elements.
 *  Features bow, blow, strike exciters, modal/string resonators, and Moog filter.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <cmath>

#include "unit.h"
#include "modal_synth.h"

class ElementsSynth {
public:
    ElementsSynth() : initialized_(false) {}

    int8_t Init(const unit_runtime_desc_t* desc) {
        runtime_desc_ = desc;
        
        // Initialize synth
        synth_.Init();
        
        // Set up default parameters (matching header.c)
        // Page 1: Exciter Mix
        params_[0] = 0;    // BOW
        params_[1] = 0;    // BLOW
        params_[2] = 100;  // STRIKE
        params_[3] = 0;    // MALLET (bipolar: -64 to +63)
        
        // Page 2: Exciter Timbre
        params_[4] = 0;    // BOW TIMBRE (bipolar)
        params_[5] = 0;    // BLOW TIMBRE (bipolar)
        params_[6] = 0;    // STK MODE (SAMPLE)
        params_[7] = 0;    // GRANULAR DENSITY (bipolar)
        
        // Page 3: Resonator
        params_[8] = 0;    // GEOMETRY (bipolar)
        params_[9] = 0;    // BRIGHTNESS (bipolar)
        params_[10] = 0;   // DAMPING (bipolar)
        params_[11] = 0;   // POSITION (bipolar)
        
        // Page 4: Filter & Model
        params_[12] = 127; // CUTOFF
        params_[13] = 0;   // RESONANCE
        params_[14] = 64;  // FLT ENV (filter envelope amount)
        params_[15] = 0;   // MODEL
        
        // Page 5: Envelope (ADR)
        params_[16] = 5;   // ATTACK
        params_[17] = 40;  // DECAY
        params_[18] = 40;  // RELEASE
        params_[19] = 0;   // ENV MODE (ADR)
        
        // Page 6: LFO
        params_[20] = 40;  // LFO RATE
        params_[21] = 0;   // LFO DEPTH (off by default)
        params_[22] = 0;   // LFO PRESET (OFF)
        params_[23] = 0;   // COARSE (bipolar: -64 to +63)
        
        // Apply initial parameters
        for (int i = 0; i < kNumParams; ++i) {
            applyParameter(i);
        }
        
        // Force resonator to recalculate coefficients with new parameters
        synth_.ForceResonatorUpdate();
        
        initialized_ = true;
        return k_unit_err_none;
    }

    void Teardown() {
        initialized_ = false;
    }

    void Reset() {
        synth_.Reset();
    }

    void Resume() {}
    void Suspend() {}

    void Render(float* out, uint32_t frames) {
        // Safety: if not initialized, output silence
        if (!initialized_) {
            for (uint32_t i = 0; i < frames * 2; ++i) {
                out[i] = 0.0f;
            }
            return;
        }
        
        // Process audio with static buffers
        static constexpr uint32_t kMaxFrames = 128;
        static float out_l[kMaxFrames];
        static float out_r[kMaxFrames];
        
        uint32_t frames_remaining = frames;
        uint32_t out_idx = 0;
        
        while (frames_remaining > 0) {
            uint32_t process_frames = (frames_remaining > kMaxFrames) ? kMaxFrames : frames_remaining;
            
            // Clear buffers before processing
            for (uint32_t i = 0; i < process_frames; ++i) {
                out_l[i] = 0.0f;
                out_r[i] = 0.0f;
            }
            
            // Call synth - this is where bad values might come from
            synth_.Process(out_l, out_r, process_frames);
            
            // ULTRA-AGGRESSIVE protection and output
            for (uint32_t i = 0; i < process_frames; ++i) {
                float l = out_l[i];
                float r = out_r[i];
                
                // 1. Check for NaN using bit pattern (most reliable)
                union { float f; uint32_t u; } ul, ur;
                ul.f = l;
                ur.f = r;
                
                // NaN has exponent all 1s and non-zero mantissa
                // Inf has exponent all 1s and zero mantissa
                // Both have (exp & 0x7F800000) == 0x7F800000
                bool l_bad = ((ul.u & 0x7F800000) == 0x7F800000);
                bool r_bad = ((ur.u & 0x7F800000) == 0x7F800000);
                
                if (l_bad) l = 0.0f;
                if (r_bad) r = 0.0f;
                
                // 2. Clamp to very conservative range
                if (l > 0.95f) l = 0.95f;
                if (l < -0.95f) l = -0.95f;
                if (r > 0.95f) r = 0.95f;
                if (r < -0.95f) r = -0.95f;
                
                out[out_idx++] = l;
                out[out_idx++] = r;
            }
            
            frames_remaining -= process_frames;
        }
    }

    void setParameter(uint8_t id, int32_t value) {
        if (id >= kNumParams) return;
        params_[id] = value;
        applyParameter(id);
    }

    int32_t getParameterValue(uint8_t id) {
        if (id >= kNumParams) return 0;
        return params_[id];
    }

    const char* getParameterStrValue(uint8_t id, int32_t value) {
        // MALLET parameter (id 3) - 12 variants: sample + timbre
        if (id == 3) {
            static const char* mallet_names[] = {
                "SOFT DK",   // 0: mallet_soft dark
                "SOFT BR",   // 1: mallet_soft bright
                "MED DK",    // 2: mallet_med dark
                "MED BR",    // 3: mallet_med bright
                "HARD DK",   // 4: mallet_hard dark
                "HARD BR",   // 5: mallet_hard bright
                "PLEC DK",   // 6: plectrum dark
                "PLEC BR",   // 7: plectrum bright
                "STIK DK",   // 8: stick dark
                "STIK BR",   // 9: stick bright
                "BOW DK",    // 10: bow_attack dark
                "BOW BR"     // 11: bow_attack bright
            };
            if (value >= 0 && value <= 11) {
                return mallet_names[value];
            }
        }
        // STK MODE parameter (id 6)
        if (id == 6) {
            static const char* mode_names[] = {"SAMPLE", "GRANULAR", "NOISE", "PLECTRUM", "PARTICLE"};
            if (value >= 0 && value <= 4) {
                return mode_names[value];
            }
        }
        // MODEL parameter (id 15)
        if (id == 15) {
            static const char* model_names[] = {"MODAL", "STRING", "MSTRING"};
            if (value >= 0 && value <= 2) {
                return model_names[value];
            }
        }
        // ENV MODE parameter (id 19)
        if (id == 19) {
            static const char* env_names[] = {"ADR", "AD", "AR", "LOOP"};
            if (value >= 0 && value <= 3) {
                return env_names[value];
            }
        }
        // LFO PRESET parameter (id 22) - Shape + Destination combos
        if (id == 22) {
            static const char* lfo_names[] = {
                "OFF",      // 0: LFO off
                "TRI>CUT",  // 1: Triangle -> Cutoff (classic filter sweep)
                "SIN>GEO",  // 2: Sine -> Geometry (smooth morph)
                "SQR>POS",  // 3: Square -> Position (rhythmic position)
                "TRI>BRI",  // 4: Triangle -> Brightness (shimmer)
                "SIN>SPC",  // 5: Sine -> Space (stereo movement)
                "SAW>CUT",  // 6: Saw -> Cutoff (evolving filter)
                "RND>SPC"   // 7: Random S&H -> Space (chaos)
            };
            if (value >= 0 && value <= 7) {
                return lfo_names[value];
            }
        }
        return nullptr;
    }

    const uint8_t* getParameterBmpValue(uint8_t id, int32_t value) {
        (void)id;
        (void)value;
        return nullptr;
    }

    void SetTempo(uint32_t tempo) {
        tempo_ = tempo;
    }

    void NoteOn(uint8_t note, uint8_t velocity) {
        if (velocity == 0) {
            NoteOff(note);
            return;
        }
        
        // Apply coarse tuning and pitch bend
        float tuned_note = (float)note + coarse_tune_ + pitch_bend_;
        current_note_ = note;
        
        synth_.NoteOn((uint8_t)tuned_note, velocity);
    }

    void NoteOff(uint8_t note) {
        if (note == current_note_) {
            synth_.NoteOff();
        }
    }

    void GateOn(uint8_t velocity) {
        float tuned_note = (float)current_note_ + coarse_tune_;
        synth_.NoteOn((uint8_t)tuned_note, velocity);
    }

    void GateOff() {
        synth_.NoteOff();
    }

    void AllNoteOff() {
        synth_.NoteOff();
        synth_.Reset();
    }

    void PitchBend(uint16_t bend) {
        // bend is 0-16383, center is 8192
        pitch_bend_ = ((float)bend - 8192.0f) / 8192.0f * 2.0f;  // +/- 2 semitones
    }

    void ChannelPressure(uint8_t pressure) {
        // Could modulate bow pressure or other params
        (void)pressure;
    }

    void Aftertouch(uint8_t note, uint8_t aftertouch) {
        (void)note;
        (void)aftertouch;
    }

    void LoadPreset(uint8_t idx) {
        preset_index_ = idx;
        
        // Preset format:
        // bow, blow, strike, mallet, bowT, blwT, stkMode, granD,
        // geo, bright, damp, pos, cutoff, reso, fltEnv, model,
        // atk, dec, rel, envMode, lfoRt, lfoDepth, lfoPre
        // Bipolar values: -64 to +63
        switch (idx) {
            case 0: // Init - Basic mallet hit
                setPresetParams(0, 0, 100, 0,  0, 0, 0, 0,
                               0, 0, 0, 0,  127, 0, 64, 0,
                               5, 40, 40, 0,  40, 0, 0);
                break;
            case 1: // Bowed String
                setPresetParams(100, 0, 0, 0,  -24, 0, 0, 0,
                               -64, -14, -34, -14,  90, 20, 40, 0,
                               30, 60, 60, 2,  40, 0, 0);
                break;
            case 2: // Bell
                setPresetParams(0, 0, 100, 26,  0, 0, 0, 0,
                               63, 26, -49, 0,  127, 0, 80, 0,
                               2, 80, 80, 1,  40, 0, 0);
                break;
            case 3: // Wobble Bass (LFO on cutoff: TRI>CUT preset)
                setPresetParams(0, 0, 100, 0,  0, 0, 0, 0,
                               -34, 6, -24, 0,  80, 50, 90, 0,
                               5, 50, 50, 0,  60, 90, 1);
                break;
            case 4: // Blown Tube
                setPresetParams(0, 100, 0, 0,  0, -14, 0, 0,
                               -44, -4, -14, 0,  70, 30, 50, 0,
                               40, 30, 40, 2,  40, 0, 0);
                break;
            case 5: // Shimmer (LFO on brightness: TRI>BRI preset)
                setPresetParams(0, 0, 100, -34,  0, 0, 0, 0,
                               16, 6, -39, -24,  100, 10, 60, 0,
                               5, 60, 70, 0,  50, 80, 4);
                break;
            case 6: // Pluck String
                setPresetParams(0, 0, 90, 36,  0, 0, 0, 0,
                               -64, 16, -4, -14,  100, 0, 100, 1,
                               2, 20, 30, 1,  40, 0, 0);
                break;
            case 7: // Drone (looping envelope, LFO on geometry)
                setPresetParams(30, 30, 40, -14,  -14, 6, 2, 36,
                               -14, -24, -44, 0,  60, 50, 30, 0,
                               60, 60, 60, 3,  30, 100, 2);
                break;
        }
    }

    uint8_t getPresetIndex() const {
        return preset_index_;
    }

    static const char* getPresetName(uint8_t idx) {
        static const char* names[] = {
            "Init",
            "Bowed Str",
            "Bell",
            "Wobble",
            "Blown Tube",
            "Shimmer",
            "Pluck Str",
            "Drone"
        };
        return (idx < 8) ? names[idx] : "";
    }

private:
    static constexpr int kNumParams = 24;
    
    // Preset function matching new parameter layout (FLT ENV on p4, LFO presets on p6)
    void setPresetParams(int bow, int blow, int strike, int mallet,
                         int bowT, int blwT, int stkMode, int granD,
                         int geo, int bright, int damp, int pos,
                         int cutoff, int reso, int fltEnv, int model,
                         int atk, int dec, int rel, int envMode,
                         int lfoRt, int lfoDepth, int lfoPre) {
        // Page 1: Exciter Mix
        params_[0] = bow;
        params_[1] = blow;
        params_[2] = strike;
        params_[3] = mallet;
        
        // Page 2: Exciter Timbre
        params_[4] = bowT;
        params_[5] = blwT;
        params_[6] = stkMode;
        params_[7] = granD;
        
        // Page 3: Resonator
        params_[8] = geo;
        params_[9] = bright;
        params_[10] = damp;
        params_[11] = pos;
        
        // Page 4: Filter & Model
        params_[12] = cutoff;
        params_[13] = reso;
        params_[14] = fltEnv;
        params_[15] = model;
        
        // Page 5: Envelope (ADR)
        params_[16] = atk;
        params_[17] = dec;
        params_[18] = rel;
        params_[19] = envMode;
        
        // Page 6: LFO
        params_[20] = lfoRt;
        params_[21] = lfoDepth;
        params_[22] = lfoPre;
        params_[23] = 0;  // COARSE at center (0 semitones, bipolar range -64 to +63)
        
        for (int i = 0; i < kNumParams; ++i) {
            applyParameter(i);
        }
    }
    
    void applyParameter(uint8_t id) {
        // For unipolar params (0-127): norm = value / 127
        // For bipolar params (-64 to +63): norm = (value + 64) / 127
        float norm = (float)params_[id] / 127.0f;  // Default for unipolar
        float bipolar_norm = (float)(params_[id] + 64) / 127.0f;  // For bipolar params
        
        switch (id) {
            // Page 1: Exciter Mix
            case 0: // BOW (unipolar)
                synth_.SetBow(norm);
                break;
            case 1: // BLOW (unipolar)
                synth_.SetBlow(norm);
                break;
            case 2: // STRIKE (unipolar)
                synth_.SetStrike(norm);
                break;
            case 3: // MALLET (enum 0-5, selects strike sample)
                synth_.SetStrikeSample(params_[id]);
                break;
                
            // Page 2: Exciter Timbre
            case 4: // BOW TIMBRE (bipolar)
                synth_.SetBowTimbre(bipolar_norm);
                break;
            case 5: // BLOW TIMBRE (bipolar)
                synth_.SetBlowTimbre(bipolar_norm);
                break;
            case 6: // STK MODE (enum)
                synth_.SetStrikeMode(params_[id]);
                break;
            case 7: // GRANULAR DENSITY (bipolar)
                synth_.SetGranularDensity(bipolar_norm);
                break;
                
            // Page 3: Resonator (all bipolar)
            case 8: // GEOMETRY
                synth_.SetStructure(bipolar_norm);
                break;
            case 9: // BRIGHTNESS
                synth_.SetBrightness(bipolar_norm);
                break;
            case 10: // DAMPING
                synth_.SetDamping(bipolar_norm);
                break;
            case 11: // POSITION
                synth_.SetPosition(bipolar_norm);
                break;
                
            // Page 4: Filter & Model
            case 12: // CUTOFF
                synth_.SetFilterCutoff(norm);
                break;
            case 13: // RESONANCE
                synth_.SetFilterResonance(norm);
                break;
            case 14: // FLT ENV (filter envelope amount)
                synth_.SetFilterEnvAmount(norm);
                break;
            case 15: // MODEL
                synth_.SetModel(params_[id]);
                break;
                
            // Page 5: Envelope (ADR)
            case 16: // ATTACK
                synth_.SetAttack(norm);
                break;
            case 17: // DECAY
                synth_.SetDecay(norm);
                break;
            case 18: // RELEASE
                synth_.SetRelease(norm);
                break;
            case 19: // ENV MODE
                synth_.SetEnvMode(params_[id]);
                break;
                
            // Page 6: LFO
            case 20: // LFO RATE
                synth_.SetLfoRate(norm);
                break;
            case 21: // LFO DEPTH
                synth_.SetLfoDepth(norm);
                break;
            case 22: // LFO PRESET (shape + destination)
                synth_.SetLfoPreset(params_[id]);
                break;
            case 23: // COARSE (bipolar: -64 to +63 maps to -24 to +24 semitones)
                coarse_tune_ = (float)params_[id] * 24.0f / 63.0f;
                break;
            default:
                break;
        }
    }

    const unit_runtime_desc_t* runtime_desc_;  // Cached for potential future use
    modal::ModalSynth synth_;
    int32_t params_[kNumParams];
    
    uint8_t current_note_ = 60;
    uint8_t preset_index_ = 0;
    uint32_t tempo_ = 120 << 16;
    
    float coarse_tune_ = 0.0f;
    float pitch_bend_ = 0.0f;
    
    bool initialized_;
};
