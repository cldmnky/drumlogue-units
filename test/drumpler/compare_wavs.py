#!/usr/bin/env python3
import struct

print("=== ORIGINAL (distorted) ===")
with open('test_output_fixed.wav', 'rb') as f:
    f.seek(0)
    header = f.read(100)
    data_pos = header.find(b'data')
    f.seek(data_pos + 8 + 48000 * 2 * 4)  
    data = f.read(16000)  # More samples
    samples = [struct.unpack('<f', data[i:i+4])[0] for i in range(0, len(data), 4)]
    
    L = [samples[i] for i in range(0, len(samples), 2)]
    R = [samples[i] for i in range(1, len(samples), 2)]
    
    L_jumps = sum(1 for i in range(1, len(L)) if abs(L[i] - L[i-1]) > 0.3)
    R_jumps = sum(1 for i in range(1, len(R)) if abs(R[i] - R[i-1]) > 0.3)
    
    print(f"L channel: {L_jumps} jumps (>0.3) in {len(L)} samples")
    print(f"R channel: {R_jumps} jumps (>0.3) in {len(R)} samples")
    print(f"L samples [100:110]: {[f'{s:.3f}' for s in L[100:110]]}")
    print(f"R samples [100:110]: {[f'{s:.3f}' for s in R[100:110]]}")

print("\n=== SWAPPED (fixed?) ===")
with open('test_output_swapped.wav', 'rb') as f:
    f.seek(0)
    header = f.read(100)
    data_pos = header.find(b'data')
    f.seek(data_pos + 8 + 48000 * 2 * 4)  
    data = f.read(16000)
    samples = [struct.unpack('<f', data[i:i+4])[0] for i in range(0, len(data), 4)]
    
    L = [samples[i] for i in range(0, len(samples), 2)]
    R = [samples[i] for i in range(1, len(samples), 2)]
    
    L_jumps = sum(1 for i in range(1, len(L)) if abs(L[i] - L[i-1]) > 0.3)
    R_jumps = sum(1 for i in range(1, len(R)) if abs(R[i] - R[i-1]) > 0.3)
    
    print(f"L channel: {L_jumps} jumps (>0.3) in {len(L)} samples")
    print(f"R channel: {R_jumps} jumps (>0.3) in {len(R)} samples")
    print(f"L samples [100:110]: {[f'{s:.3f}' for s in L[100:110]]}")
    print(f"R samples [100:110]: {[f'{s:.3f}' for s in R[100:110]]}")
