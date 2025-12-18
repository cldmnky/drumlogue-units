/**
 * @file jupiter_env.cc
 * @brief ADSR Envelope Generator implementation
 *
 * Based on Bristol envelope.c
 * Linear ADSR segments with quadratic parameter-to-time mapping
 */

#include "jupiter_env.h"
#include <algorithm>

#ifdef DEBUG
#include <cstdio>
#endif

namespace dsp {

JupiterEnvelope::JupiterEnvelope()
    : sample_rate_(48000.0f)
    , state_(STATE_IDLE)
    , current_level_(0.0f)
    , target_level_(0.0f)
    , velocity_(1.0f)
    , attack_time_(0.001f)
    , decay_time_(0.1f)
    , sustain_level_(0.7f)
    , release_time_(0.05f)
    , attack_rate_(0.0f)
    , decay_coef_(0.0f)
    , release_coef_(0.0f)
{
}

JupiterEnvelope::~JupiterEnvelope() {
}

void JupiterEnvelope::Init(float sample_rate) {
    sample_rate_ = sample_rate;
    Reset();
    UpdateRates();
}

void JupiterEnvelope::SetAttack(float time_sec) {
    attack_time_ = std::max(kMinTime, std::min(time_sec, kMaxTime));
#ifdef DEBUG
    fprintf(stderr, "[Envelope SetAttack] input=%.6fs -> clamped=%.6fs (kMinTime=%.6fs)\n",
            time_sec, attack_time_, kMinTime);
    fflush(stderr);
#endif
    UpdateRates();
}

void JupiterEnvelope::SetDecay(float time_sec) {
    decay_time_ = std::max(kMinTime, std::min(time_sec, kMaxTime));
#ifdef DEBUG
    fprintf(stderr, "[Envelope SetDecay] input=%.6fs -> clamped=%.6fs (kMinTime=%.6fs)\n",
            time_sec, decay_time_, kMinTime);
    fflush(stderr);
#endif
    UpdateRates();
}

void JupiterEnvelope::SetSustain(float level) {
    sustain_level_ = std::max(0.0f, std::min(level, 1.0f));
}

void JupiterEnvelope::SetRelease(float time_sec) {
    release_time_ = std::max(kMinTime, std::min(time_sec, kMaxTime));
#ifdef DEBUG
    fprintf(stderr, "[Envelope SetRelease] input=%.6fs -> clamped=%.6fs (kMinTime=%.6fs)\n",
            time_sec, release_time_, kMinTime);
    fflush(stderr);
#endif
    UpdateRates();
}

void JupiterEnvelope::NoteOn(float velocity) {
    velocity_ = std::max(0.0f, std::min(velocity, 1.0f));
    state_ = STATE_ATTACK;
    target_level_ = 1.0f;
    // Don't reset current_level_ - allow retriggering from current position
#ifdef DEBUG
    fprintf(stderr, "[Envelope] NoteOn: velocity=%.3f, sustain_level=%.3f, A=%.3fs, D=%.3fs, R=%.3fs\n",
            velocity_, sustain_level_, attack_time_, decay_time_, release_time_);
    fflush(stderr);
#endif
}

void JupiterEnvelope::NoteOff() {
    if (state_ != STATE_IDLE) {
        state_ = STATE_RELEASE;
        target_level_ = 0.0f;
    }
}

float JupiterEnvelope::Process() {
#ifdef DEBUG
    static int debug_counter = 0;
    static State last_logged_state = STATE_IDLE;
    static int state_change_limiter = 0;
    
    // Log state changes (rate-limited to avoid spam)
    if (state_ != last_logged_state) {
        if (++state_change_limiter >= 4800) {  // Max 10 state change logs per second
            state_change_limiter = 0;
            const char* state_names[] = {"IDLE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
            fprintf(stderr, "[Envelope] State: %s -> %s (level=%.3f, target=%.3f)\n",
                    state_names[last_logged_state], state_names[state_],
                    current_level_, target_level_);
            fflush(stderr);
        }
        last_logged_state = state_;
    } else {
        state_change_limiter = 0;  // Reset limiter when state is stable
    }
    
    // Log periodically (once per second)
    if (++debug_counter >= 48000) {
        debug_counter = 0;
        const char* state_names[] = {"IDLE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
        fprintf(stderr, "[Envelope Process] state=%s, level=%.6f, sustain=%.3f\n",
                state_names[state_], current_level_, sustain_level_);
        fflush(stderr);
    }
#endif

    switch (state_) {
        case STATE_ATTACK:
            current_level_ += attack_rate_;
            if (current_level_ >= 1.0f) {
                current_level_ = 1.0f;
                state_ = STATE_DECAY;
                target_level_ = sustain_level_;
            }
            break;
        
        case STATE_DECAY:
            // Exponential decay toward sustain level
            current_level_ = sustain_level_ + (current_level_ - sustain_level_) * decay_coef_;
            if (current_level_ <= sustain_level_ + 0.0001f) { // Threshold to snap
                current_level_ = sustain_level_;
                state_ = STATE_SUSTAIN;
            }
            break;
        
        case STATE_SUSTAIN:
            current_level_ = sustain_level_;
            break;
        
        case STATE_RELEASE:
            // Exponential release toward 0
            current_level_ *= release_coef_;
            if (current_level_ <= 0.0001f) { // Threshold to snap to silence
                current_level_ = 0.0f;
                state_ = STATE_IDLE;
            }
            break;
        
        case STATE_IDLE:
        default:
            current_level_ = 0.0f;
            break;
    }
    
    return current_level_ * velocity_;
}

void JupiterEnvelope::Reset() {
    state_ = STATE_IDLE;
    current_level_ = 0.0f;
    target_level_ = 0.0f;
}

void JupiterEnvelope::UpdateRates() {
    attack_rate_ = TimeToRate(attack_time_);
    decay_coef_ = TimeToCoef(decay_time_);
    release_coef_ = TimeToCoef(release_time_);
}

float JupiterEnvelope::TimeToRate(float time_sec) const {
    // Rate = increment per sample to go from 0 to 1 in time_sec
    if (time_sec < kMinTime) {
        time_sec = kMinTime;
    }
    return 1.0f / (time_sec * sample_rate_);
}

float JupiterEnvelope::TimeToCoef(float time_sec) const {
    if (time_sec < kMinTime) {
        return 0.0f; // Instant decay
    }
    // Exponential decay to ~1% (-40dB) in time_sec
    // coef = exp(ln(0.01) / (time * fs))
    // ln(0.01) approx -4.605
    return expf(-4.605f / (time_sec * sample_rate_));
}

} // namespace dsp
