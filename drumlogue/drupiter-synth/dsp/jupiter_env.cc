/**
 * @file jupiter_env.cc
 * @brief ADSR Envelope Generator implementation
 *
 * Based on Bristol envelope.c
 * Four-stage envelope with linear segments (simplified from exponential)
 */

#include "jupiter_env.h"
#include <algorithm>

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
    , decay_rate_(0.0f)
    , release_rate_(0.0f)
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
    UpdateRates();
}

void JupiterEnvelope::SetDecay(float time_sec) {
    decay_time_ = std::max(kMinTime, std::min(time_sec, kMaxTime));
    UpdateRates();
}

void JupiterEnvelope::SetSustain(float level) {
    sustain_level_ = std::max(0.0f, std::min(level, 1.0f));
}

void JupiterEnvelope::SetRelease(float time_sec) {
    release_time_ = std::max(kMinTime, std::min(time_sec, kMaxTime));
    UpdateRates();
}

void JupiterEnvelope::NoteOn(float velocity) {
    velocity_ = std::max(0.0f, std::min(velocity, 1.0f));
    state_ = STATE_ATTACK;
    target_level_ = 1.0f;
    // Don't reset current_level_ - allow retriggering from current position
}

void JupiterEnvelope::NoteOff() {
    if (state_ != STATE_IDLE) {
        state_ = STATE_RELEASE;
        target_level_ = 0.0f;
    }
}

float JupiterEnvelope::Process() {
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
            current_level_ -= decay_rate_;
            if (current_level_ <= sustain_level_) {
                current_level_ = sustain_level_;
                state_ = STATE_SUSTAIN;
            }
            break;
        
        case STATE_SUSTAIN:
            current_level_ = sustain_level_;
            break;
        
        case STATE_RELEASE:
            current_level_ -= release_rate_;
            if (current_level_ <= 0.0f) {
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
    decay_rate_ = TimeToRate(decay_time_);
    release_rate_ = TimeToRate(release_time_);
}

float JupiterEnvelope::TimeToRate(float time_sec) const {
    // Rate = increment per sample to go from 0 to 1 in time_sec
    if (time_sec < kMinTime) {
        time_sec = kMinTime;
    }
    return 1.0f / (time_sec * sample_rate_);
}

} // namespace dsp
