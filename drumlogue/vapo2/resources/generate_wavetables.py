#!/usr/bin/env python3
"""
Generate wavetables.h from PPG Wave 2.2 waveform data.

This script parses ppg_data.c and converts the 8-bit, 64-sample PPG waveforms
into 16-bit, 256-sample wavetables suitable for the Vapo2 synth.

The conversion process:
1. Parse the raw PPG waveform data (8-bit unsigned, 64 samples)
2. Convert to signed 16-bit (-32768 to 32767)
3. Interpolate from 64 to 256 samples using cubic interpolation
4. Optionally integrate for anti-aliased playback (Franck & Välimäki method)
5. Add guard samples for interpolation

PPG Wave 2.2 had ~110 waveforms organized into wavetables. We select
representative waves to create 4 banks of 8 waves each.
"""

import re
import numpy as np
from pathlib import Path
from scipy import interpolate

# Configuration
PPG_SAMPLES = 64
TARGET_SAMPLES = 256
GUARD_SAMPLES = 4
WAVES_PER_BANK = 8
NUM_BANKS = 16  # Expanded to 16 banks

# Selected waveforms for each bank (indices into ppg_waveforms_data)
# PPG Wave 2.2 has 256 waveforms (0-255), but many above 120 are silent/utility
# These are chosen to provide good morphing transitions within each bank
BANK_SELECTIONS = {
    # Bank 0: UPPER WT - Original PPG Upper Wavetable character
    "UPPER_WT": [0, 1, 2, 3, 4, 5, 6, 7],
    
    # Bank 1: RESONANT1 - Resonant/filtered sounds
    "RESONANT1": [8, 9, 10, 11, 12, 13, 14, 15],
    
    # Bank 2: RESONANT2 - More resonant variations
    "RESONANT2": [16, 17, 18, 19, 20, 21, 22, 23],
    
    # Bank 3: MELLOW - Softer, warmer tones
    "MELLOW": [24, 25, 26, 27, 28, 29, 30, 31],
    
    # Bank 4: BRIGHT - Brighter, more harmonics
    "BRIGHT": [32, 33, 34, 35, 36, 37, 38, 39],
    
    # Bank 5: HARSH - Aggressive, distorted character
    "HARSH": [40, 41, 42, 43, 44, 45, 46, 47],
    
    # Bank 6: CLIPPER - Clipped/squared waveforms
    "CLIPPER": [48, 49, 50, 51, 52, 53, 54, 55],
    
    # Bank 7: SYNC - Oscillator sync-like timbres
    "SYNC": [56, 57, 58, 59, 60, 61, 62, 63],
    
    # Bank 8: PWM - Pulse width modulation-like
    "PWM": [64, 65, 66, 67, 68, 69, 70, 71],
    
    # Bank 9: VOCAL1 - Formant/vocal sounds
    "VOCAL1": [72, 73, 74, 75, 76, 77, 78, 79],
    
    # Bank 10: VOCAL2 - More vocal variations  
    "VOCAL2": [80, 81, 82, 83, 84, 85, 86, 87],
    
    # Bank 11: ORGAN - Organ-like timbres
    "ORGAN": [88, 89, 90, 91, 92, 93, 94, 95],
    
    # Bank 12: BELL - Bell/metallic tones
    "BELL": [96, 97, 98, 99, 100, 101, 102, 103],
    
    # Bank 13: ALIEN - Strange, otherworldly
    "ALIEN": [104, 105, 106, 107, 108, 109, 110, 111],
    
    # Bank 14: NOISE - Noisy, textural
    "NOISE": [112, 113, 114, 115, 116, 117, 118, 119],
    
    # Bank 15: SPECIAL - Special/unique waveforms
    "SPECIAL": [120, 121, 122, 123, 124, 125, 126, 127],
}


def parse_ppg_waveforms(filepath: Path) -> list[np.ndarray]:
    """Parse ppg_data.c and extract all waveforms."""
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Find the ppg_waveforms_data array
    match = re.search(r'ppg_waveforms_data\[\].*?=\s*\{([^;]+)\};', content, re.DOTALL)
    if not match:
        raise ValueError("Could not find ppg_waveforms_data in file")
    
    data_section = match.group(1)
    
    # Extract all numeric values (ignoring comments)
    values = []
    for line in data_section.split('\n'):
        # Remove comments
        line = re.sub(r'//.*$', '', line)
        # Find numbers
        numbers = re.findall(r'\d+', line)
        values.extend(int(n) for n in numbers)
    
    # Split into 64-sample waveforms
    waveforms = []
    for i in range(0, len(values), PPG_SAMPLES):
        if i + PPG_SAMPLES <= len(values):
            waveforms.append(np.array(values[i:i + PPG_SAMPLES], dtype=np.uint8))
    
    print(f"Parsed {len(waveforms)} waveforms from PPG data")
    return waveforms


def convert_to_signed16(wave: np.ndarray) -> np.ndarray:
    """Convert 8-bit unsigned (0-255, center 128) to 16-bit signed."""
    # Center at 0 and scale to 16-bit range
    signed = (wave.astype(np.float64) - 128.0) * 256.0
    return np.clip(signed, -32767, 32767).astype(np.int16)


def interpolate_wave(wave: np.ndarray, target_samples: int) -> np.ndarray:
    """Interpolate waveform to target size using cubic interpolation."""
    x_old = np.linspace(0, 1, len(wave), endpoint=False)
    x_new = np.linspace(0, 1, target_samples, endpoint=False)
    
    # Use cubic interpolation for smooth results
    # Wrap-around for periodic signal
    wave_extended = np.concatenate([wave, wave[:4]])
    x_extended = np.concatenate([x_old, x_old[:4] + 1.0])
    
    f = interpolate.interp1d(x_extended, wave_extended, kind='cubic')
    return f(x_new)


def integrate_wave(wave: np.ndarray) -> np.ndarray:
    """
    Integrate waveform for anti-aliased playback.
    
    This implements the Franck & Välimäki "Higher-order integrated Wavetable 
    Synthesis" technique. The oscillator will differentiate during playback,
    which naturally provides anti-aliasing.
    """
    # Cumulative sum (integration)
    integrated = np.cumsum(wave.astype(np.float64))
    
    # Remove DC offset by subtracting linear ramp
    n = len(integrated)
    dc_ramp = np.linspace(0, integrated[-1], n)
    integrated = integrated - dc_ramp
    
    # Normalize to fit in int16 range with headroom
    max_val = np.max(np.abs(integrated))
    if max_val > 0:
        integrated = integrated * (30000.0 / max_val)
    
    return np.clip(integrated, -32767, 32767).astype(np.int16)


def add_guard_samples(wave: np.ndarray, num_guard: int = GUARD_SAMPLES) -> np.ndarray:
    """Add guard samples at the end for interpolation."""
    return np.concatenate([wave, wave[:num_guard]])


def format_wave_array(name: str, wave: np.ndarray, samples_per_line: int = 10) -> str:
    """Format a waveform as a C array."""
    lines = [f"static const int16_t {name}[WT_SAMPLES_PER_WAVE] = {{"]
    
    values = wave.tolist()
    for i in range(0, len(values), samples_per_line):
        chunk = values[i:i + samples_per_line]
        line = "    " + ", ".join(f"{v:6d}" for v in chunk)
        if i + samples_per_line < len(values):
            line += ","
        lines.append(line)
    
    lines.append("};")
    return "\n".join(lines)


def generate_wavetables_h(waveforms: list[np.ndarray], output_path: Path, integrate: bool = False):
    """Generate the wavetables.h file."""
    
    num_banks = len(BANK_SELECTIONS)
    bank_names_list = list(BANK_SELECTIONS.keys())
    
    # Generate bank descriptions for header
    bank_desc = "\n".join([f" * - {name}: PPG waves {indices[0]}-{indices[-1]}" 
                           for name, indices in BANK_SELECTIONS.items()])
    
    header = f'''/**
 * @file wavetables.h
 * @brief Pre-computed wavetable data for Vapo2
 *
 * Generated from PPG Wave 2.2 waveform data.
 * Original PPG waves: 64 samples, 8-bit unsigned
 * Converted to: 256 samples, 16-bit signed
 *
 * {num_banks} Banks with 8 waves each:
{bank_desc}
 *
 * Each bank has 8 waves for smooth morphing.
 */

#pragma once

#include <cstdint>

// Wavetable dimensions
#define WT_TABLE_SIZE 256
#define WT_WAVES_PER_BANK 8
#define WT_NUM_BANKS {num_banks}
#define WT_SAMPLES_PER_WAVE (WT_TABLE_SIZE + 4)  // +4 for interpolation guard

'''
    
    sections = []
    bank_pointers = []
    
    for bank_idx, (bank_name, wave_indices) in enumerate(BANK_SELECTIONS.items()):
        section_lines = [
            f"// ============================================================================",
            f"// BANK {bank_idx}: {bank_name}",
            f"// ============================================================================",
            ""
        ]
        
        wave_names = []
        for i, wave_idx in enumerate(wave_indices):
            if wave_idx >= len(waveforms):
                print(f"Warning: Wave {wave_idx} not found, using wave 0")
                wave_idx = 0
            
            # Process the waveform
            raw = waveforms[wave_idx]
            signed16 = convert_to_signed16(raw)
            interpolated = interpolate_wave(signed16, TARGET_SAMPLES)
            
            if integrate:
                processed = integrate_wave(interpolated)
            else:
                processed = np.clip(interpolated, -32767, 32767).astype(np.int16)
            
            with_guard = add_guard_samples(processed)
            
            # Generate array name
            wave_name = f"wt_{bank_name.lower()}_{i}"
            wave_names.append(wave_name)
            
            section_lines.append(f"// PPG Wave {wave_idx}")
            section_lines.append(format_wave_array(wave_name, with_guard))
            section_lines.append("")
        
        sections.append("\n".join(section_lines))
        
        # Bank pointer array
        bank_ptr_name = f"wavetable_bank_{bank_name.lower()}"
        ptr_lines = [
            f"static const int16_t* const {bank_ptr_name}[WT_WAVES_PER_BANK + 1] = {{"
        ]
        for name in wave_names:
            ptr_lines.append(f"    {name},")
        ptr_lines.append(f"    {wave_names[0]}  // Guard for interpolation")
        ptr_lines.append("};")
        bank_pointers.append("\n".join(ptr_lines))
    
    # Master bank array - dynamically generate based on BANK_SELECTIONS
    bank_ptr_names = [f"wavetable_bank_{name.lower()}" for name in BANK_SELECTIONS.keys()]
    bank_names_quoted = [f'    "{name}"' for name in BANK_SELECTIONS.keys()]
    
    master_array = f'''
// ============================================================================
// Master bank array
// ============================================================================
static const int16_t* const* const wavetable_banks[WT_NUM_BANKS] = {{
{chr(10).join(f"    {name}," for name in bank_ptr_names)}
}};

// Bank names for display (max 8 chars for drumlogue display)
static const char* const wavetable_bank_names[WT_NUM_BANKS] = {{
{chr(10).join(f'{name},' for name in bank_names_quoted)}
}};
'''
    
    # Write the file
    with open(output_path, 'w') as f:
        f.write(header)
        for section in sections:
            f.write(section)
            f.write("\n")
        
        f.write("// ============================================================================\n")
        f.write("// Bank pointer arrays\n")
        f.write("// ============================================================================\n\n")
        
        for ptr in bank_pointers:
            f.write(ptr)
            f.write("\n\n")
        
        f.write(master_array)
    
    print(f"Generated {output_path}")
    
    # Calculate size
    total_samples = NUM_BANKS * WAVES_PER_BANK * (TARGET_SAMPLES + GUARD_SAMPLES)
    total_bytes = total_samples * 2  # int16_t = 2 bytes
    print(f"Total wavetable data: {total_bytes} bytes ({total_bytes / 1024:.1f} KB)")


def main():
    script_dir = Path(__file__).parent
    ppg_data_path = script_dir / "ppg_data.c"
    output_path = script_dir / "wavetables.h"
    
    if not ppg_data_path.exists():
        print(f"Error: {ppg_data_path} not found")
        return 1
    
    print("Parsing PPG waveform data...")
    waveforms = parse_ppg_waveforms(ppg_data_path)
    
    print(f"\nBank selections:")
    for bank_name, indices in BANK_SELECTIONS.items():
        print(f"  {bank_name}: waves {indices}")
    
    print("\nGenerating wavetables.h...")
    # Set integrate=False for now - the oscillator can handle raw waveforms
    # Set integrate=True if using differentiation-based anti-aliasing
    generate_wavetables_h(waveforms, output_path, integrate=False)
    
    return 0


if __name__ == "__main__":
    exit(main())
