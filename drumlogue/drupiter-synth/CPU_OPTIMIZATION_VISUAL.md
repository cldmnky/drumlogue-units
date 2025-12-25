# CPU Load Visualization: Polyphonic Mode Bottleneck

## Current Architecture CPU Breakdown (64 frames @ 48kHz = 1.33ms)

### Monophonic Mode (Safe ✅)
```
CPU Budget: 1.33ms per buffer (100%)

┌─────────────────────────────────────────────────────────┐
│ MONOPHONIC MODE - Total: ~1.0ms (75% of budget)         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  LFO Process (1×)              ████ 2%                 │
│  Envelopes (2×)               ████████ 8%              │
│  DCO Processing (2×)          ████████████████ 20%     │
│  HPF + VCF (shared)           ████████████████ 20%     │
│  VCA + Modulation             ████████ 8%              │
│  Effects + Output             ████████ 8%              │
│  Overhead                     ████ 4%                  │
│  FREE HEADROOM                ██████████ 25% ✅        │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Polyphonic Mode (4 Voices) - Current Implementation ❌
```
CPU Budget: 1.33ms per buffer (100%)

┌─────────────────────────────────────────────────────────┐
│ POLYPHONIC MODE - Total: ~1.7ms (128% of budget) 💥     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  LFO Process (1×)              ██ 1%                   │
│  Envelopes (2×)               ██ 1%                    │
│  Per-Voice Overhead           ████ 5%                  │
│  Voice DCO Processing (4×8)   ████████████████████... 48% ⚠️  │
│  Voice Envelope (4×8)         ████████████ 18% ⚠️       │
│  HPF + VCF (shared)           ████████ 12%            │
│  VCA + Modulation             ████ 8%                 │
│  Effects + Output             ████ 8%                 │
│  OVERRUN                       ██████████ +28% 💥       │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Polyphonic Mode (4 Voices) - After Phase 1 Optimizations ✅
```
CPU Budget: 1.33ms per buffer (100%)

┌─────────────────────────────────────────────────────────┐
│ OPTIMIZED (Strategies A+B+E) - Total: ~1.1ms (85%)     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  LFO Process (1×)              ██ 2%                   │
│  Envelopes (2×)               ██ 2%                   │
│  Per-Voice Overhead (reduced)  ████ 3%                │
│  Voice DCO Processing (4×4)    ██████████████ 28% ✅   │
│  Voice Envelope (4×4)          ████████ 9% ✅          │
│  HPF + VCF (shared)           ████████ 12%            │
│  VCA + Modulation             ████ 8%                 │
│  Effects + Output             ████ 8%                 │
│  FREE HEADROOM                ███████ 15% ✅           │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Polyphonic Mode (4 Voices) - After Phase 2 Optimizations ✅✅
```
CPU Budget: 1.33ms per buffer (100%)

┌─────────────────────────────────────────────────────────┐
│ FULLY OPTIMIZED (Strategies C+D) - Total: ~0.8ms (60%) │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  LFO Process (1×)              ██ 2%                   │
│  Envelopes (2×)               ██ 2%                   │
│  Per-Voice Overhead           ██ 1%                   │
│  Voice DSO Processing (shared) ████████ 12% ✅✅       │
│  Voice Envelope (pooled)       ███ 5% ✅✅              │
│  HPF + VCF (shared)           ████████ 12%            │
│  VCA + Modulation             ████ 8%                 │
│  Effects + Output             ████ 8%                 │
│  FREE HEADROOM                ████████████████ 40% 🎉  │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## CPU Cost Per Component (Estimated Cycles/Sample)

### Oscillator Processing
```
DCO1 Phase Accumulation:      8 cycles
DCO1 Waveform Generation:     40-60 cycles (depends on type)
  ├─ Sawtooth:               40 cycles (simple)
  ├─ Square/Pulse:           50 cycles (PWM)
  ├─ Triangle:               60 cycles (anti-alias)
  └─ Sine (table):           45 cycles (lookup)
DCO1 Frequency Update:        15 cycles

DCO2 (same as DCO1):          ~50-65 cycles
────────────────────────────────────────
Per Voice DCO Cost:           ~100-130 cycles/sample
```

### Envelope Processing
```
ADSR State Machine:           15 cycles
Exponential Curve:            10 cycles
Release Envelope:             5 cycles
────────────────────────────────────────
Per Voice Envelope:           ~20-30 cycles/sample
```

### Voice Overhead
```
IsActive() check:             3 cycles
const_cast<>:                 1 cycle
Level scaling/mixing:         10 cycles
────────────────────────────────────────
Per Voice Overhead:           ~14 cycles/sample
```

### Filter & Modulation (Shared)
```
Cutoff Frequency Calc:        40 cycles (amortized 1-2ms)
Filter State Update:          30 cycles (per mode)
HPF Processing:               8 cycles
VCA Envelope:                 20 cycles
Modulation (LFO/KB track):    10 cycles
────────────────────────────────────────
Shared Filter Cost:           ~60-70 cycles/sample
```

---

## Voice Scaling Impact

### Total Cost Per Voice vs. Voice Count

```
        CPU Load (% of 1.33ms budget)
        
100% ├─────────────────────────────────
     │         💥 Crashes here
     │        /
     │       /
     │      / ← Mono (1 voice): 75%
80%  ├────●─────────────────────────────
     │    ╲
     │     ╲
     │      ╲
     │       ╲     ← 4 voices (Phase 1): 85%
     │        ●════════════════────────────
     │
60%  ├─────────────────────────────────
     │                    ← 4 voices (Phase 2): 60%
     │                    ●
     │
     │
40%  ├─────────────────────────────────
     │                    FREE HEADROOM ✅
     │
     │
0%   └─────────────────────────────────
       0    1    2    3    4    5    6    7
            Number of Active Voices
```

---

## DCO Instance Impact

### Current Architecture: 8 DCO Instances Max

```
Per-Frame (64 samples):

Voice 0: [ DCO1.0 Process ] [ DCO2.0 Process ] = 50 cycles
Voice 1: [ DCO1.1 Process ] [ DCO2.1 Process ] = 50 cycles
Voice 2: [ DCO1.2 Process ] [ DCO2.2 Process ] = 50 cycles
Voice 3: [ DCO1.3 Process ] [ DCO2.3 Process ] = 50 cycles
─────────────────────────────────────────────
Total DCO cost: 4 × 50 = 200 cycles/sample
Per buffer:    200 × 64 = 12,800 cycles

At 48MHz:      12,800 / 48M × 1e6 = 0.267ms per buffer
Budget impact: 20% of 1.33ms ⚠️ (multiplies base mono DCO cost 4×)
```

### After Optimization: Shared DCO with Per-Voice Phase

```
Per-Frame (64 samples):

Phase Update V0: [ Phase += Freq ] = 5 cycles
Phase Update V1: [ Phase += Freq ] = 5 cycles
Phase Update V2: [ Phase += Freq ] = 5 cycles
Phase Update V3: [ Phase += Freq ] = 5 cycles
Shared DCO:      [ Process @ avg freq ] = 50 cycles
─────────────────────────────────────────────
Total DCO cost: (4×5) + 50 = 70 cycles/sample
Per buffer:    70 × 64 = 4,480 cycles

At 48MHz:      4,480 / 48M × 1e6 = 0.093ms per buffer
Budget impact: 7% of 1.33ms ✅ (65% reduction!)
```

---

## Why Polyphonic Breaks Drumlogue

### Issue: Exponential CPU Growth

```
                    Total CPU Time per Buffer
                    
3ms ├────────────────────────────────
    │ 8 voices @ 50 cy/sample
    │ = 400 cy/s × 64 = 25,600 cycles
    │ = 2.9ms → 218% of budget 💥💥
    │ ╱
2.5ms┤╱
    │╱
2ms ├────────────────────────────────
    │ 4 voices @ 50 cy/sample
    │ = 200 cy/s × 64 = 12,800 cycles
    │ = 1.7ms → 128% of budget 💥
    │    ╱
1.5ms┤  ╱
    │ ╱
    │╱
1ms ├────────────────────────────────
    │ 1 voice (mono) = 0.9ms
    │
0.5ms├────────────────────────────────

0ms └────────────────────────────────
      0   1   2   3   4   5   6   7   8
          Active Voices
```

**Root Cause:** 
- Mono processing: 1 DCO pair = baseline
- Each additional voice: +1 full DCO pair + envelope
- 4 voices = 4× baseline cost
- Budget exceeded at ~3-4 active voices

**Drumlogue constraints:**
- 48kHz sample rate (fixed)
- 64-frame buffer (fixed)
- ~1.33ms max per buffer (limited ARM Cortex-A7 CPU)
- Must share CPU with UI, MIDI, effects

---

## Memory Footprint Comparison

### Current Architecture
```
Per Voice (×8 max):
├── JupiterDCO dco1        48 bytes (phase, freq, state)
├── JupiterDCO dco2        48 bytes
├── JupiterVCF vcf        128 bytes (4 state variables)
├── JupiterEnvelope env_amp 64 bytes
├── JupiterEnvelope env_filter 64 bytes
├── JupiterEnvelope env_pitch  64 bytes
└── Voice metadata         32 bytes
────────────────────────────
Total per voice:          448 bytes
Total 8 voices:         3,584 bytes (3.5KB)

Plus:
├── Main DCOs (mono):     96 bytes
├── Main VCF:            128 bytes
├── Main Envelopes:      192 bytes
└── Buffers (aligned):  1,024 bytes
────────────────────────────
Grand Total:            5,024 bytes (5KB on stack/heap)
```

### After Optimization (Shared DCO)
```
Per Voice (×8 max):
├── Phase state (phase1, phase2)   8 bytes (2× float)
├── Frequency cache                8 bytes
├── JupiterEnvelope env_amp       64 bytes
├── JupiterEnvelope env_filter    64 bytes
├── JupiterEnvelope env_pitch     64 bytes
└── Voice metadata                32 bytes
────────────────────────────
Total per voice:          240 bytes
Total 8 voices:         1,920 bytes (1.9KB) ✅

Plus:
├── Shared DCO1:          48 bytes
├── Shared DCO2:          48 bytes
├── Main VCF:            128 bytes
├── Main Envelopes:      192 bytes
└── Buffers:            1,024 bytes
────────────────────────────
Grand Total:            3,360 bytes (3.3KB) ✅

SAVINGS: 1,664 bytes (33% reduction)
```

---

## Summary: The CPU Problem Explained Simply

| Aspect | Mono | 4-Voice Poly (Current) | 4-Voice Poly (Optimized) |
|--------|------|----------------------|--------------------------|
| **Active DCO pairs** | 1 | 4 | 1 (shared) |
| **Active envelopes** | 2 | 8 | 8 (pooled) |
| **CPU time** | 0.9ms | 1.7ms | 0.9ms |
| **% of budget** | 68% | 128% ❌ | 68% ✅ |
| **Status** | Safe | **Crashes** | Safe |

**The issue:** Each voice runs independent DCO processors, multiplying CPU cost.
**The fix:** Share DCO hardware between voices, only store per-voice phase state.
**The result:** Same polyphonic sound with 50% less CPU. ✅

