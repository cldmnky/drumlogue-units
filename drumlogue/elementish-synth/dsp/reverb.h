/*
 * Schroeder Reverb - lightweight reverb for the Elements-style SPACE control
 * Part of Modal Synth for Drumlogue
 *
 * Fixed-size parallel comb + series allpass network. All buffers are static so
 * no dynamic allocation is required (real-time safe).
 */

#pragma once

#include "dsp_core.h"

namespace modal {

class Reverb {
public:
    static constexpr int kCombCount = 8;
    static constexpr int kAllpassCount = 4;
    static constexpr int kMaxCombDelay = 1024;
    static constexpr int kMaxAllpassDelay = 256;

    Reverb() {
        Init();
    }

    void Init() {
        for (int i = 0; i < kCombCount; ++i) {
            comb_index_[i] = 0;
            comb_filter_state_[i] = 0.0f;
        }
        for (int i = 0; i < kAllpassCount; ++i) {
            allpass_index_[i] = 0;
        }
        for (int c = 0; c < kCombCount; ++c) {
            for (int i = 0; i < kMaxCombDelay; ++i) {
                comb_buffer_[c][i] = 0.0f;
            }
        }
        for (int a = 0; a < kAllpassCount; ++a) {
            for (int i = 0; i < kMaxAllpassDelay; ++i) {
                allpass_buffer_[a][i] = 0.0f;
            }
        }
        amount_ = 0.0f;
        time_ = 0.5f;
        diffusion_ = 0.7f;
        input_gain_ = 0.2f;
        lp_coeff_ = 0.3f;
    }

    // Wet amount 0-1 (how much reverb signal is audible)
    void set_amount(float a) { amount_ = Clamp(a, 0.0f, 1.0f); }

    // Decay time 0-1 (feedback of the comb filters)
    void set_time(float t) { time_ = Clamp(t, 0.0f, 1.0f); }

    // Diffusion 0-1 (allpass feedback)
    void set_diffusion(float d) { diffusion_ = Clamp(d, 0.0f, 1.0f); }

    // Input gain (drive level into the reverb tank)
    void set_input_gain(float g) { input_gain_ = Clamp(g, 0.0f, 1.0f); }

    // One-pole low-pass damping; 0 = dark, 1 = bright
    void set_lp(float lp) {
        lp = Clamp(lp, 0.0f, 1.0f);
        lp_coeff_ = 0.1f + lp * 0.85f;
    }

    void Reset() {
        Init();
    }

    // Wet/dry stereo process. `dry` is the input signal (and dry output),
    // `wet` receives the reverb tail. Caller mixes wet back into the output.
    void Process(float* dry, float* wet, uint32_t frames) {
        if (frames == 0) return;

        float fb_comb = 0.70f + time_ * 0.25f;   // 0.70 .. 0.95
        float ap_gain = diffusion_ * 0.5f;       // 0.0 .. 0.5
        float wet_gain = amount_ * input_gain_ * 4.0f;
        if (wet_gain > 1.0f) wet_gain = 1.0f;

        for (uint32_t i = 0; i < frames; ++i) {
            float in = dry[i];
            if (IsBad(in)) in = 0.0f;

            // Parallel comb filters.
            float acc = 0.0f;
            for (int c = 0; c < kCombCount; ++c) {
                int len = kCombDelay[c];
                int idx = comb_index_[c] + len;
                if (idx >= kMaxCombDelay) idx -= kMaxCombDelay;
                float delayed = comb_buffer_[c][idx];
                if (IsBad(delayed)) {
                    delayed = 0.0f;
                    comb_buffer_[c][idx] = 0.0f;
                }

                // One-pole damping filter in the feedback path.
                float filtered = comb_filter_state_[c] +
                    lp_coeff_ * (delayed - comb_filter_state_[c]);
                if (IsBad(filtered)) filtered = 0.0f;
                comb_filter_state_[c] = filtered;

                comb_buffer_[c][comb_index_[c]] = in * input_gain_ + filtered * fb_comb;
                if (IsBad(comb_buffer_[c][comb_index_[c]])) {
                    comb_buffer_[c][comb_index_[c]] = 0.0f;
                }
                comb_index_[c]++;
                if (comb_index_[c] >= kMaxCombDelay) comb_index_[c] = 0;
                acc += filtered;
            }
            acc *= 0.125f;  // Normalize over 8 combs

            // Series allpass filters for echo density.
            for (int a = 0; a < kAllpassCount; ++a) {
                int len = kAllpassDelay[a];
                int idx = allpass_index_[a] + len;
                if (idx >= kMaxAllpassDelay) idx -= kMaxAllpassDelay;
                float delayed = allpass_buffer_[a][idx];
                if (IsBad(delayed)) {
                    delayed = 0.0f;
                    allpass_buffer_[a][idx] = 0.0f;
                }
                // Standard Schroeder allpass: bounded for |ap_gain| < 1.
                float ap_out = -ap_gain * acc + delayed;
                allpass_buffer_[a][allpass_index_[a]] = acc + ap_gain * ap_out;
                if (IsBad(allpass_buffer_[a][allpass_index_[a]])) {
                    allpass_buffer_[a][allpass_index_[a]] = 0.0f;
                }
                allpass_index_[a]++;
                if (allpass_index_[a] >= kMaxAllpassDelay) allpass_index_[a] = 0;
                acc = ap_out;
            }

            wet[i] = acc * wet_gain;
#ifdef UNIT_HOST_NATIVE
            debug_last_wet_ = wet[i];
            if (IsBad(wet[i])) debug_nonfinite_ = true;
#endif
            if (IsBad(wet[i])) wet[i] = 0.0f;
        }
    }

#ifdef UNIT_HOST_NATIVE
    float DebugLastWet() const { return debug_last_wet_; }
    bool DebugNonfinite() const { return debug_nonfinite_; }
#endif

private:
    static bool IsBad(float value) {
        union {
            float f;
            uint32_t u;
        } bits = {value};
        const bool nonfinite = (bits.u & 0x7F800000u) == 0x7F800000u;
        return nonfinite || value > 1.0e6f || value < -1.0e6f;
    }

    // Separate delay lines prevent comb/allpass filters from overwriting each
    // other's feedback state. Total memory is about 36KB.
    static float comb_buffer_[kCombCount][kMaxCombDelay];
    static float allpass_buffer_[kAllpassCount][kMaxAllpassDelay];

    static const int kCombDelay[kCombCount];
    static const int kAllpassDelay[kAllpassCount];

    int comb_index_[kCombCount];
    int allpass_index_[kAllpassCount];
    float comb_filter_state_[kCombCount];

    float amount_;
    float time_;
    float diffusion_;
    float input_gain_;
    float lp_coeff_;

#ifdef UNIT_HOST_NATIVE
    float debug_last_wet_ = 0.0f;
    bool debug_nonfinite_ = false;
#endif
};

float Reverb::comb_buffer_[Reverb::kCombCount][Reverb::kMaxCombDelay];
float Reverb::allpass_buffer_[Reverb::kAllpassCount][Reverb::kMaxAllpassDelay];

// Scaled, non-multiple-of-each-other prime-ish delays (in samples at 48kHz).
const int Reverb::kCombDelay[Reverb::kCombCount] = {
    401, 463, 557, 631, 709, 787, 863, 941
};
const int Reverb::kAllpassDelay[Reverb::kAllpassCount] = {
    113, 157, 197, 251
};

} // namespace modal
