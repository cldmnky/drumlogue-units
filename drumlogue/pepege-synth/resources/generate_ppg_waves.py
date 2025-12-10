#!/usr/bin/env python3
"""
Generate ppg_waves.cc from ppg_data.c

Extracts the 8-bit waveform data from the original usynth ppg_data.c
and creates a C++ source file for use with the PPG oscillator.
"""

import re
from pathlib import Path

def extract_waveforms(ppg_data_path):
    """Extract waveform data from ppg_data.c"""
    with open(ppg_data_path, 'r') as f:
        content = f.read()
    
    # Find the ppg_waveforms_data array
    match = re.search(r'ppg_waveforms_data\[\]\s*PROGMEM\s*=\s*\{([^}]+)\}', content, re.DOTALL)
    if not match:
        raise ValueError("Could not find ppg_waveforms_data in ppg_data.c")
    
    # Parse the values
    data_str = match.group(1)
    values = []
    for line in data_str.split('\n'):
        # Remove comments
        line = re.sub(r'//.*', '', line)
        # Extract numbers
        numbers = re.findall(r'\d+', line)
        values.extend([int(n) for n in numbers])
    
    return values

def generate_cpp(waveforms, output_path):
    """Generate the C++ source file with wave data"""
    
    num_waves = len(waveforms) // 64
    
    output = '''/**
 * @file ppg_waves.cc
 * @brief PPG Wave 2.2 8-bit waveform data
 *
 * Auto-generated from ppg_data.c
 * 
 * Contains 128 waveforms, each 64 samples of 8-bit unsigned data.
 * The PPG oscillator mirrors these half-cycles to create full 128-sample waves.
 */

#include "ppg_waves.h"

// 128 waveforms x 64 samples = 8192 bytes
const uint8_t ppg_wave_data[PPG_NUM_WAVES * PPG_SAMPLES_PER_WAVE] = {
'''
    
    for wave_idx in range(num_waves):
        base = wave_idx * 64
        wave_data = waveforms[base:base + 64]
        
        output += f'    // Wave {wave_idx:3d} ({wave_idx // 8}:{wave_idx % 8})\n'
        
        # Write 8 values per line
        for row in range(8):
            row_data = wave_data[row * 8:(row + 1) * 8]
            values_str = ', '.join(f'{v:3d}' for v in row_data)
            if wave_idx < num_waves - 1 or row < 7:
                output += f'    {values_str},\n'
            else:
                output += f'    {values_str}\n'
    
    output += '};\n'
    
    with open(output_path, 'w') as f:
        f.write(output)
    
    print(f"Generated {output_path}")
    print(f"  - {num_waves} waveforms")
    print(f"  - {len(waveforms)} total samples")

def main():
    script_dir = Path(__file__).parent
    ppg_data_path = script_dir / "ppg_data.c"
    output_path = script_dir / "ppg_waves.cc"
    
    if not ppg_data_path.exists():
        print(f"Error: {ppg_data_path} not found")
        return 1
    
    waveforms = extract_waveforms(ppg_data_path)
    generate_cpp(waveforms, output_path)
    
    return 0

if __name__ == "__main__":
    exit(main())
