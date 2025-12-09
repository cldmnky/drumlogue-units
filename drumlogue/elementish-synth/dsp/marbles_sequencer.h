/**
 * @file marbles_sequencer.h
 * @brief Marbles-inspired generative sequencer for drumlogue
 *
 * Inspired by Mutable Instruments Marbles "déjà vu" concept.
 * Generates tempo-synced random notes with looping capability.
 * 
 * Option B: Clock-Synced Subdivisions
 * - Pattern step triggers the sequencer
 * - Sequencer generates notes at subdivisions of the beat
 * - SLOW = 1 note per beat, MED = 2, FAST = 4, X2 = 8, X4 = 16
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace marbles {

// Sequencer presets combining rate and scale
enum SeqPreset {
    SEQ_OFF = 0,      // Sequencer disabled (pass-through)
    SEQ_SLOW,         // 1 note per beat (same as input)
    SEQ_MED,          // 2 notes per beat
    SEQ_FAST,         // 4 notes per beat
    SEQ_X2,           // 8 notes per beat
    SEQ_X4,           // 16 notes per beat
    SEQ_MAJ,          // Major scale, 4 notes/beat
    SEQ_MIN,          // Minor scale, 4 notes/beat
    SEQ_PENT,         // Pentatonic, 4 notes/beat
    SEQ_CHROM,        // Chromatic, 4 notes/beat
    SEQ_OCT,          // Octaves only, 2 notes/beat
    SEQ_5TH,          // Perfect fifths, 2 notes/beat
    SEQ_4TH,          // Perfect fourths, 2 notes/beat
    SEQ_TRI,          // Triads, 3 notes/beat
    SEQ_7TH,          // 7th chord tones, 4 notes/beat
    SEQ_RAND,         // Random scale, 4 notes/beat
    SEQ_NUM_PRESETS
};

// Scale definitions (semitone offsets from root)
struct Scale {
    const int8_t* notes;
    uint8_t length;
};

// Scale note arrays
static const int8_t kScaleChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const int8_t kScaleMajor[] = {0, 2, 4, 5, 7, 9, 11};
static const int8_t kScaleMinor[] = {0, 2, 3, 5, 7, 8, 10};
static const int8_t kScalePentatonic[] = {0, 2, 4, 7, 9};
// Interval scales: Use out-of-octave offsets (e.g., -12, 12) to create large
// melodic jumps. These are offsets applied to the base note, allowing octave
// leaps, fifths below (-5 = fifth down), and other wide intervals.
static const int8_t kScaleOctaves[] = {0, 12, -12};
static const int8_t kScaleFifths[] = {0, 7, -5, 12};
static const int8_t kScaleFourths[] = {0, 5, -7, 12};
static const int8_t kScaleTriad[] = {0, 4, 7, 12};
static const int8_t kScaleSeventh[] = {0, 4, 7, 10, 11};

// Loop buffer size for déjà vu
static const int kLoopBufferSize = 8;

// Note queue size - enough for max subdivisions per buffer
static const int kNoteQueueSize = 8;

class MarblesSequencer {
public:
    MarblesSequencer() : sample_rate_(48000.0f), enabled_(false), active_(false),
                         note_queue_read_(0), note_queue_write_(0) {}

    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        phase_ = 0.0f;
        phase_increment_ = 0.0f;
        tempo_ = 120 << 16;  // Default 120 BPM in 16.16 fixed point
        
        // Initialize RNG with a seed
        rng_state_ = 0x12345678;
        
        // Clear loop buffer
        for (int i = 0; i < kLoopBufferSize; ++i) {
            loop_buffer_[i] = 0.5f;
        }
        loop_index_ = 0;
        
        // Defaults
        preset_ = SEQ_OFF;
        enabled_ = false;
        active_ = false;
        deja_vu_ = 0.0f;
        spread_ = 0.5f;
        base_note_ = 60;  // Middle C
        base_velocity_ = 100;
        transpose_ = 0;
        
        subdivisions_remaining_ = 0;
        subdivision_count_ = 1;
        
        // Clear note queue
        note_queue_read_ = 0;
        note_queue_write_ = 0;
        
        UpdateClockRate();
    }

    void SetTempo(uint32_t tempo) {
        tempo_ = tempo;
        UpdateClockRate();
    }

    /**
     * Called when a pattern step triggers a note.
     * This starts/restarts the subdivision sequence.
     */
    void Trigger(uint8_t note, uint8_t velocity) {
        base_note_ = note;
        base_velocity_ = velocity;
        
        if (!enabled_) return;
        
        // Start subdivision sequence
        active_ = true;
        phase_ = 0.0f;
        subdivisions_remaining_ = subdivision_count_;
        
        // Generate the first note immediately
        GenerateNote();
    }

    /**
     * Called when note off received.
     * Stops any remaining subdivisions.
     */
    void Release() {
        active_ = false;
        subdivisions_remaining_ = 0;
    }

    /**
     * Process audio frames - generates subdivision notes.
     * Uses skip-ahead optimization instead of per-sample iteration.
     * Multiple notes per buffer are queued to prevent loss.
     */
    void Process(uint32_t frames) {
        if (!enabled_ || !active_ || subdivisions_remaining_ <= 0) return;
        if (phase_increment_ <= 0.0f) return;  // Avoid infinite loop
        
        // Calculate total phase advance for this buffer
        float total_advance = phase_increment_ * static_cast<float>(frames);
        float current_phase = phase_;
        
        // Count triggers that will occur in this buffer using skip-ahead
        while (total_advance > 0.0f && subdivisions_remaining_ > 0) {
            // Distance to next trigger
            float distance_to_trigger = 1.0f - current_phase;
            
            if (distance_to_trigger <= total_advance) {
                // Trigger occurs within this buffer
                total_advance -= distance_to_trigger;
                current_phase = 0.0f;  // Reset phase after trigger
                subdivisions_remaining_--;
                
                if (subdivisions_remaining_ > 0) {
                    GenerateNote();
                } else {
                    // Done with subdivisions for this beat
                    active_ = false;
                    break;
                }
            } else {
                // No more triggers in this buffer, just advance phase
                current_phase += total_advance;
                break;
            }
        }
        
        phase_ = current_phase;
    }

    /**
     * Get next queued note. Returns false if queue is empty.
     */
    bool GetNextNote(uint8_t* note, uint8_t* velocity) {
        if (note_queue_read_ == note_queue_write_) return false;  // Queue empty
        
        *note = note_queue_[note_queue_read_].note;
        *velocity = note_queue_[note_queue_read_].velocity;
        note_queue_read_ = (note_queue_read_ + 1) % kNoteQueueSize;
        return true;
    }

    void SetBaseNote(uint8_t note) {
        base_note_ = note;
    }

    void SetPreset(int preset) {
        // Bounds checking before casting
        if (preset < 0 || preset >= SEQ_NUM_PRESETS) {
            preset = SEQ_OFF;
        }
        preset_ = static_cast<SeqPreset>(preset);
        enabled_ = (preset_ != SEQ_OFF);
        UpdateClockRate();
    }

    void SetSpread(float spread) {
        spread_ = spread;  // 0.0 to 1.0
    }

    void SetDejaVu(float deja_vu) {
        deja_vu_ = deja_vu;  // 0.0 to 1.0
    }

    void SetTranspose(int semitones) {
        transpose_ = semitones;
    }

    bool IsEnabled() const {
        return enabled_;
    }

    // Get preset name for UI
    static const char* GetPresetName(int preset) {
        static const char* names[] = {
            "OFF", "SLOW", "MED", "FAST", "X2", "X4",
            "MAJ", "MIN", "PENT", "CHROM", "OCT", "5TH",
            "4TH", "TRI", "7TH", "RAND"
        };
        if (preset >= 0 && preset < SEQ_NUM_PRESETS) {
            return names[preset];
        }
        return "???";
    }

private:
    void UpdateClockRate() {
        // Convert tempo from 16.16 fixed point to BPM
        float bpm = static_cast<float>(tempo_) / 65536.0f;
        float beats_per_second = bpm / 60.0f;
        
        // Determine subdivisions per beat based on preset
        switch (preset_) {
            case SEQ_OFF:
                subdivision_count_ = 1;
                break;
            case SEQ_SLOW:
                subdivision_count_ = 1;   // 1 note per beat
                break;
            case SEQ_MED:
            case SEQ_OCT:
            case SEQ_5TH:
            case SEQ_4TH:
                subdivision_count_ = 2;   // 2 notes per beat
                break;
            case SEQ_TRI:
                subdivision_count_ = 3;   // 3 notes per beat (triplet feel)
                break;
            case SEQ_FAST:
            case SEQ_MAJ:
            case SEQ_MIN:
            case SEQ_PENT:
            case SEQ_CHROM:
            case SEQ_7TH:
            case SEQ_RAND:
                subdivision_count_ = 4;   // 4 notes per beat (16th notes)
                break;
            case SEQ_X2:
                subdivision_count_ = 8;   // 8 notes per beat (32nd notes)
                break;
            case SEQ_X4:
                subdivision_count_ = 16;  // 16 notes per beat
                break;
            default:
                subdivision_count_ = 4;
                break;
        }
        
        // Phase increment for subdivisions within one beat
        // At the pattern trigger, we have 1 beat to fit subdivision_count_ notes
        float subdivisions_per_second = beats_per_second * static_cast<float>(subdivision_count_);
        phase_increment_ = subdivisions_per_second / sample_rate_;
    }

    void GenerateNote() {
        // Get random value (0.0 to 1.0)
        float random_value;
        
        // Déjà vu logic: probability of replaying from loop
        if (RandomFloat() < deja_vu_) {
            // Replay from loop buffer
            random_value = loop_buffer_[loop_index_];
        } else {
            // Generate new random value and store
            random_value = RandomFloat();
            loop_buffer_[loop_index_] = random_value;
        }
        
        // Advance loop index
        loop_index_ = (loop_index_ + 1) % kLoopBufferSize;
        
        // Convert random value to note offset
        // spread_ controls the range: 0 = ±0 semitones, 1 = ±24 semitones
        float range = spread_ * 24.0f;
        float offset = (random_value - 0.5f) * 2.0f * range;
        
        // Quantize to scale
        int quantized_offset = QuantizeToScale(static_cast<int>(offset));
        
        // Calculate final note
        int final_note = base_note_ + quantized_offset + transpose_;
        
        // Clamp to valid MIDI range
        if (final_note < 0) final_note = 0;
        if (final_note > 127) final_note = 127;
        
        // Velocity: use base velocity with slight random variation
        int vel_variation = static_cast<int>((RandomFloat() - 0.5f) * 30.0f);
        int final_velocity = base_velocity_ + vel_variation;
        if (final_velocity < 1) final_velocity = 1;
        if (final_velocity > 127) final_velocity = 127;
        
        // Push to note queue (circular buffer)
        uint8_t next_write = (note_queue_write_ + 1) % kNoteQueueSize;
        if (next_write != note_queue_read_) {  // Check for queue full
            note_queue_[note_queue_write_].note = static_cast<uint8_t>(final_note);
            note_queue_[note_queue_write_].velocity = static_cast<uint8_t>(final_velocity);
            note_queue_write_ = next_write;
        }
        // If queue is full, note is dropped (shouldn't happen with proper queue size)
    }

    int QuantizeToScale(int semitones) {
        const Scale* scale = GetCurrentScale();
        if (!scale || scale->length == 0) {
            return semitones;  // No quantization
        }
        
        // Find octave and position within octave
        int octave = 0;
        int semi = semitones;
        
        if (semi >= 0) {
            octave = semi / 12;
            semi = semi % 12;
        } else {
            // Handle negative semitones
            octave = (semi - 11) / 12;
            semi = semi - octave * 12;
        }
        
        // Find closest scale degree
        int closest = scale->notes[0];
        int min_dist = 100;
        
        for (int i = 0; i < scale->length; ++i) {
            int note = scale->notes[i];
            if (note < 0) note += 12;  // Handle negative offsets
            if (note >= 12) note -= 12;
            
            int dist = semi - note;
            if (dist < 0) dist = -dist;
            
            // Also check wrapping
            int dist_wrap = 12 - dist;
            if (dist_wrap < dist) dist = dist_wrap;
            
            if (dist < min_dist) {
                min_dist = dist;
                closest = note;
            }
        }
        
        return octave * 12 + closest;
    }

    const Scale* GetCurrentScale() {
        static Scale chromatic = {kScaleChromatic, 12};
        static Scale major = {kScaleMajor, 7};
        static Scale minor = {kScaleMinor, 7};
        static Scale pentatonic = {kScalePentatonic, 5};
        static Scale octaves = {kScaleOctaves, 3};
        static Scale fifths = {kScaleFifths, 4};
        static Scale fourths = {kScaleFourths, 4};
        static Scale triad = {kScaleTriad, 4};
        static Scale seventh = {kScaleSeventh, 5};
        
        switch (preset_) {
            case SEQ_MAJ:
                return &major;
            case SEQ_MIN:
                return &minor;
            case SEQ_PENT:
                return &pentatonic;
            case SEQ_CHROM:
                return &chromatic;
            case SEQ_OCT:
                return &octaves;
            case SEQ_5TH:
                return &fifths;
            case SEQ_4TH:
                return &fourths;
            case SEQ_TRI:
                return &triad;
            case SEQ_7TH:
                return &seventh;
            case SEQ_RAND:
                // Pick random scale
                {
                    int idx = static_cast<int>(RandomFloat() * 5.0f);
                    static const Scale* scales[] = {&major, &minor, &pentatonic, &chromatic, &fifths};
                    return scales[idx % 5];
                }
            default:
                return &chromatic;  // Default to chromatic (no real quantization)
        }
    }

    uint32_t Random() {
        // Linear congruential generator
        rng_state_ = rng_state_ * 1664525UL + 1013904223UL;
        return rng_state_;
    }

    float RandomFloat() {
        return static_cast<float>(Random()) / 4294967296.0f;
    }

    // State
    float sample_rate_;
    float phase_;
    float phase_increment_;
    uint32_t tempo_;
    
    // Random generator
    uint32_t rng_state_;
    
    // Déjà vu loop buffer
    float loop_buffer_[kLoopBufferSize];
    int loop_index_;
    float deja_vu_;
    
    // Parameters
    SeqPreset preset_;
    bool enabled_;
    bool active_;           // Currently generating subdivisions
    float spread_;
    uint8_t base_note_;
    uint8_t base_velocity_;
    int transpose_;
    
    // Subdivision tracking
    int subdivisions_remaining_;
    int subdivision_count_;
    
    // Note queue (circular buffer to prevent note loss when multiple
    // subdivisions trigger within one Process() call)
    struct QueuedNote {
        uint8_t note;
        uint8_t velocity;
    };
    QueuedNote note_queue_[kNoteQueueSize];
    uint8_t note_queue_read_;
    uint8_t note_queue_write_;
};

}  // namespace marbles
