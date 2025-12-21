/**
 * @file presets.h
 * @brief Factory presets for Clouds RevFX
 *
 * Defines 8 factory presets covering the full spectrum of Clouds-inspired effects:
 * - INIT: Clean starting point
 * - HALL: Large concert hall reverb
 * - PLATE: Bright plate reverb
 * - SHIMMER: Ethereal octave-up reverb
 * - CLOUD: Dense granular texture cloud
 * - FREEZE: Infinite sustain pad machine
 * - OCTAVER: Sub-octave thickener
 * - AMBIENT: Evolving ambient wash
 */

#pragma once

#include <cstdint>

namespace presets {

// Direct preset structure (no template dependency)
struct CloudsPreset {
    char name[14];              // Preset name (13 chars + null, drumlogue limit)
    int32_t params[24];         // 24 parameter values
};

// Factory presets array - 8 presets covering diverse sonic territories
static const CloudsPreset kFactoryPresets[8] = {
    // Preset 0: INIT - Clean, neutral reverb starting point
    // Medium mix, moderate decay, balanced diffusion, no extras
    {
        "INIT",
        {100, 70, 70, 100, 50, 0, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0, 0, 64, 0, 0, 0, 64, 0, 0}
    },
    
    // Preset 1: HALL - Large concert hall, natural and spacious
    // Long decay, high diffusion, slightly dark (LP~80), subtle texture for air
    {
        "HALL",
        {110, 115, 100, 80, 45, 20, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0, 0, 64, 0, 0, 0, 64, 0, 0}
    },
    
    // Preset 2: PLATE - Classic bright plate reverb
    // Short-medium decay, max diffusion (dense), very bright, no texture/grain
    {
        "PLATE",
        {100, 55, 127, 127, 55, 0, 0, 64, 64, 64, 64, 0, 0, 64, 64, 0, 0, 64, 0, 0, 0, 64, 0, 0}
    },
    
    // Preset 3: SHIMMER - Ethereal octave-up reverb
    // Long decay, moderate diffusion, bright, strong pitch shift (+12 semis = 76)
    // Subtle grain adds sparkle, slow LFO on shift pitch for movement
    {
        "SHIMMER",
        {120, 105, 85, 110, 40, 30, 25, 80, 50, 76, 64, 0, 90, 76, 85, 0, 13, 25, 40, 0, 0, 64, 0, 0}
    },
    
    // Preset 4: CLOUD - Dense granular texture cloud
    // Heavy grain processing, moderate reverb, slow LFO on grain position
    // Creates evolving, atmospheric textures
    {
        "CLOUD", 
        {90, 85, 80, 90, 45, 50, 100, 100, 85, 64, 64, 0, 0, 64, 64, 0, 11, 20, 60, 0, 9, 35, 45, 2}
    },
    
    // Preset 5: FREEZE - Infinite sustain pad machine
    // Very long decay, high diffusion, texture for smoothness
    // Designed to capture and sustain incoming audio infinitely
    {
        "FREEZE",
        {130, 127, 110, 85, 35, 70, 40, 90, 45, 64, 64, 0, 0, 64, 64, 0, 6, 15, 35, 0, 0, 64, 0, 0}
    },
    
    // Preset 6: OCTAVER - Pitch-shifted reverb (octave down)
    // Clear pitch shift effect (-12 semis = 52), moderate reverb, crisp
    // Good for thickening bass or creating sub-octave drones
    {
        "OCTAVER",
        {100, 75, 75, 95, 55, 15, 0, 64, 64, 64, 64, 0, 100, 52, 75, 0, 0, 64, 0, 0, 0, 64, 0, 0}
    },
    
    // Preset 7: AMBIENT - Lush, evolving ambient wash
    // Long decay, warm (LP lower), texture + light grain for movement
    // Dual LFO: slow texture mod + slow grain density mod for organic evolution
    {
        "AMBIENT",
        {140, 110, 95, 70, 40, 55, 35, 85, 55, 64, 64, 0, 20, 71, 80, 0, 6, 18, 50, 0, 9, 22, 40, 0}
    }
};

}  // namespace presets