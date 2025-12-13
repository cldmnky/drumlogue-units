import numpy as np
import soundfile as sf
import os

# Create fixtures directory
os.makedirs("fixtures", exist_ok=True)

# Sample rate and duration
sr = 48000
duration = 10  # 10 seconds for profiling

print("Generating 10-second test signals for profiling...")

# 1. Impulse response (10 seconds)
impulse = np.zeros(duration * sr, dtype=np.float32)
impulse[0] = 1.0
sf.write("fixtures/impulse.wav", impulse, sr)
print("Generated fixtures/impulse.wav (10s)")

# 2. Sine waves (10 seconds) - for synth testing
freqs = [220, 440, 1000, 4000]
for freq in freqs:
    t = np.linspace(0, duration, duration * sr, False, dtype=np.float32)
    sine = 0.5 * np.sin(2 * np.pi * freq * t)
    sf.write(f"fixtures/sine_{freq}hz.wav", sine, sr)
    print(f"Generated fixtures/sine_{freq}hz.wav (10s)")

# 3. White noise (10 seconds)
noise = np.random.normal(0, 0.3, duration * sr).astype(np.float32)
sf.write("fixtures/noise.wav", noise, sr)
print("Generated fixtures/noise.wav (10s)")

# 4. Pink noise (10 seconds) - better for testing effects
pink = np.random.normal(0, 0.3, duration * sr).astype(np.float32)
# Simple pink noise approximation using running sum
for i in range(1, len(pink)):
    pink[i] = pink[i] * 0.1 + pink[i-1] * 0.9
pink = pink / np.max(np.abs(pink)) * 0.3
sf.write("fixtures/pink_noise.wav", pink, sr)
print("Generated fixtures/pink_noise.wav (10s)")

# 5. Frequency sweep (10 seconds)
t = np.linspace(0, duration, duration * sr, False, dtype=np.float32)
f_start, f_end = 20, 20000
sweep = 0.3 * np.sin(2 * np.pi * (f_start + (f_end - f_start) * t / duration) * t)
sf.write("fixtures/sweep.wav", sweep, sr)
print("Generated fixtures/sweep.wav (10s)")

# 6. Drum loop simulation (10 seconds) - rhythmic content
t = np.arange(duration * sr, dtype=np.float32) / sr
drum = np.zeros_like(t)
for i in range(0, int(duration * 4)):  # 4 hits per second
    kick_time = i * 0.25
    kick_idx = int(kick_time * sr)
    if kick_idx < len(drum):
        # Synthesize kick drum
        decay = np.exp(-50 * np.arange(int(0.3 * sr)) / sr)
        freq_env = 60 + 120 * np.exp(-40 * np.arange(int(0.3 * sr)) / sr)
        kick = 0.8 * np.sin(2 * np.pi * freq_env * np.arange(int(0.3 * sr)) / sr) * decay
        end_idx = min(kick_idx + len(kick), len(drum))
        drum[kick_idx:end_idx] += kick[:end_idx - kick_idx]
sf.write("fixtures/drums.wav", drum.astype(np.float32), sr)
print("Generated fixtures/drums.wav (10s)")

# 7. Complex harmonic content (10 seconds)
t = np.linspace(0, duration, duration * sr, False, dtype=np.float32)
complex_signal = np.zeros_like(t)
for harmonic in [1, 2, 3, 5, 7, 11]:  # Odd and prime harmonics
    complex_signal += (0.3 / harmonic) * np.sin(2 * np.pi * 440 * harmonic * t)
complex_signal = complex_signal / np.max(np.abs(complex_signal)) * 0.5
sf.write("fixtures/complex.wav", complex_signal, sr)
print("Generated fixtures/complex.wav (10s)")

# 8. Vocal-like formant content (10 seconds)
t = np.linspace(0, duration, duration * sr, False, dtype=np.float32)
vocal = 0.1 * np.sin(2 * np.pi * 150 * t)  # Fundamental
vocal += 0.05 * np.sin(2 * np.pi * 800 * t)  # First formant
vocal += 0.03 * np.sin(2 * np.pi * 1200 * t)  # Second formant
vocal += 0.02 * np.sin(2 * np.pi * 2500 * t)  # Third formant
sf.write("fixtures/vocal.wav", vocal, sr)
print("Generated fixtures/vocal.wav (10s)")

# 9. Quiet signal (10 seconds) - tests low-level processing
quiet = 0.01 * np.random.normal(0, 1, duration * sr).astype(np.float32)
sf.write("fixtures/quiet.wav", quiet, sr)
print("Generated fixtures/quiet.wav (10s)")

# 10. Loud signal (10 seconds) - tests clipping/limiting
loud = 0.9 * np.random.normal(0, 1, duration * sr).astype(np.float32)
loud = np.clip(loud, -0.95, 0.95)
sf.write("fixtures/loud.wav", loud, sr)
print("Generated fixtures/loud.wav (10s)")

print("\nAll test signals generated!")
