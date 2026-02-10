#!/usr/bin/env python3
import struct

with open('test_output_fixed.wav', 'rb') as f:
    # Find data chunk
    f.seek(0)
    header = f.read(100)
    data_pos = header.find(b'data')
    f.seek(data_pos + 8)
    
    # Skip ahead to where note should be playing
    f.seek(data_pos + 8 + 48000 * 2 * 4)  
    
    # Read stereo samples
    data = f.read(8000)
    samples = [struct.unpack('<f', data[i:i+4])[0] for i in range(0, len(data), 4)]
    
    print("Samples from 1 second into file:")
    print(f"First 20 L/R pairs:")
    for i in range(0, min(40, len(samples)), 2):
        print(f"  L={samples[i]:8.6f} R={samples[i+1]:8.6f}")
    
    print(f"\nMax: {max(samples):.6f}, Min: {min(samples):.6f}")
    print(f"Mean abs: {sum(abs(s) for s in samples)/len(samples):.6f}")
    
    # Check for large jumps
    jumps = []
    for i in range(1, len(samples)):
        diff = abs(samples[i] - samples[i-1])
        if diff > 0.5:
            jumps.append((i, samples[i-1], samples[i], diff))
    
    print(f"\nLarge discontinuities (>0.5): {len(jumps)}")
    for idx, prev, curr, diff in jumps[:5]:
        print(f"  Sample {idx}: {prev:.6f} -> {curr:.6f} (jump: {diff:.6f})")
        
    # Check for repeated values
    repeats = sum(1 for i in range(1, len(samples)) if samples[i] == samples[i-1] and abs(samples[i]) > 0.01)
    print(f"\nRepeated non-zero values: {repeats} out of {len(samples)}")
