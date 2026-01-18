# Drupiter → JP-8 Alignment Plan

Goal: make drupiter’s signal flow, voice architecture, modulation, and behavior align with Roland JP‑8, while preserving drumlogue constraints (48kHz, realtime, limited CPU/RAM).

Scope: drupiter synth only. No changes to other units.

Constraints:
- Real‑time safe: no dynamic allocation in render path.
- Preserve drumlogue SDK constraints and existing build system.
- Incremental: each phase should compile and be testable on hardware.
---

## 📋 Detailed Implementation Plan

**See [drupiter-jp8-detailed-implementation.md](drupiter-jp8-detailed-implementation.md) for:**
- Specific code locations and line numbers
- Exact changes required for each phase
- File-by-file modification guide
- Code examples with before/after comparisons

---

## 🔑 Critical Discovery (Code Audit)

**Per-voice VCF already exists but is NOT being used!**

The `Voice` struct in [voice_allocator.h](../../drumlogue/drupiter-synth/dsp/voice_allocator.h) contains:
```cpp
struct Voice {
    JupiterDCO dco1;        // ✅ Used per-voice
    JupiterDCO dco2;        // ✅ Used per-voice
    JupiterVCF vcf;         // ❌ EXISTS but NOT USED in POLY!
    JupiterEnvelope env_amp;    // ✅ Used per-voice
    JupiterEnvelope env_filter; // ❌ EXISTS but NOT USED in POLY!
    JupiterEnvelope env_pitch;  // ✅ Used per-voice
};
```

**Current POLY flow (incorrect):**
```
Voice 1: DCO1+DCO2 → env_amp → mix ─┐
Voice 2: DCO1+DCO2 → env_amp → mix ─┼→ SHARED HPF → SHARED VCF → SHARED VCA → out
Voice 3: DCO1+DCO2 → env_amp → mix ─┤
Voice 4: DCO1+DCO2 → env_amp → mix ─┘
```

**JP-8 target flow:**
```
Voice 1: DCO1+DCO2 → HPF → VCF(env_filter) → VCA(env_amp) → mix ─┐
Voice 2: DCO1+DCO2 → HPF → VCF(env_filter) → VCA(env_amp) → mix ─┼→ out
Voice 3: DCO1+DCO2 → HPF → VCF(env_filter) → VCA(env_amp) → mix ─┤
Voice 4: DCO1+DCO2 → HPF → VCF(env_filter) → VCA(env_amp) → mix ─┘
```

**Primary fix location:** [drupiter_synth.cc#L478-L595](../../drumlogue/drupiter-synth/drupiter_synth.cc) (POLY render loop)

---
## JP‑8 characteristics (sourced)
These points are drawn from public documentation and summaries to guide alignment.

- Voice architecture: 8‑voice polyphonic, 2 VCOs per voice, per‑voice HPF → VCF → VCA chain.
- Filters: non‑resonant HPF (6 dB/oct) followed by resonant LPF with selectable 12/24 dB slopes.
- Envelopes: two ADSR envelopes (ENV‑1 typically for VCF, ENV‑2 for VCA). ENV‑1 can invert.
- LFO: single LFO with multiple waveforms (triangle/saw/square/random), delay (fade‑in), and key‑trigger option.
- Mod routing: VCO modulator supports LFO and ENV‑1 to pitch, PWM source select (LFO/ENV/manual), VCF can select ENV‑1 or ENV‑2 as its mod source, VCA has a discrete LFO mod depth switch.
- Keyboard tracking: VCF key follow 0–120%; envelope key follow switches present for ENV‑1/ENV‑2.
- Oscillator features: cross‑mod (VCO2 → VCO1), sync, VCO2 low‑freq mode.

Primary sources:
- Wikipedia overview (architecture summary): https://en.wikipedia.org/wiki/Roland_Jupiter-8
- Roland technical specifications (control list and sections): https://support.roland.com/hc/en-us/articles/201945109-Jupiter-8-Technical-Specifications
- Roland JP‑8 service manual (block diagram confirms VCO → MIX → HPF → VCF → VCA per voice): https://synthfool.com/docs/Roland/Roland_Jupiter8/roland_jp8_service_manual.pdf
- Patch & Tweak analysis (mod routing, HPF/VCF structure): https://www.patchandtweak.com/jupiter-8-a-look-beneath-the-colorful-surface/

## Phase 0 — Baseline audit (current implementation)

**Status: ✅ COMPLETE** — See detailed findings above and in [drupiter-jp8-detailed-implementation.md](drupiter-jp8-detailed-implementation.md)

1. Inventory current topology and routing per mode:
   - MONO/UNISON: shared VCF/VCA; poly mixes before shared VCF/VCA.
   - POLY: per‑voice AMP ENV + velocity, but shared HPF/VCF/VCA post‑mix.
2. Identify differences vs JP‑8:
   - JP‑8 has per‑voice VCF and VCA in poly.
   - JP‑8 modulation paths are per‑voice for VCO/VCF/VCA.
3. Capture current parameter ranges and scaling for DCO, VCF, ENV, LFO, HPF, VCA.
4. Confirm current code entry points and key files:
   - drupiter_synth.cc/.h: main render and routing.
   - dsp/voice_allocator.*: voice structure and per‑voice state.
   - dsp/jupiter_vcf.*: filter implementation.
   - dsp/jupiter_env.*: envelope implementation.
   - dsp/jupiter_dco.*: oscillator implementation.

**Audit Results:**

| Component | Location | Per-Voice? | Used in POLY? |
|-----------|----------|------------|---------------|
| DCO1/DCO2 | `Voice::dco1`, `Voice::dco2` | ✅ Yes | ✅ Yes |
| VCF | `Voice::vcf` | ✅ Yes | ❌ **NO** |
| VCF ENV | `Voice::env_filter` | ✅ Yes | ❌ **NO** |
| VCA ENV | `Voice::env_amp` | ✅ Yes | ✅ Yes |
| Pitch ENV | `Voice::env_pitch` | ✅ Yes | ✅ Yes |
| HPF | `hpf_prev_output_`, `hpf_prev_input_` | ❌ No (class members) | N/A (shared) |

**Key Files:**
- Main render: [drupiter_synth.cc](../../drumlogue/drupiter-synth/drupiter_synth.cc) (1572 lines)
- Voice struct: [voice_allocator.h#L56-L72](../../drumlogue/drupiter-synth/dsp/voice_allocator.h)
- POLY loop: [drupiter_synth.cc#L478-L595](../../drumlogue/drupiter-synth/drupiter_synth.cc)
- Shared HPF: [drupiter_synth.cc#L746-L756](../../drumlogue/drupiter-synth/drupiter_synth.cc)
- Shared VCF: [drupiter_synth.cc#L795](../../drumlogue/drupiter-synth/drupiter_synth.cc)

Deliverable: ✅ Audit complete. Detailed implementation plan created.

Code touchpoints (Phase 0):
- drupiter_synth.cc: locate mono/poly render branches and current routing.
- dsp/voice_allocator.*: locate per‑voice state and allocation logic.
- dsp/jupiter_vcf.* / dsp/jupiter_env.*: confirm parameter mapping and modulation inputs.

Example (trace current routing in poly):
// drupiter_synth.cc (pseudo)
if (mode == kVoiceModePoly) {
   render_voices_to_mix();
   hpf.Process(mix);    // shared
   vcf.Process(mix);    // shared
   vca.Apply(mix);      // shared
}

## Phase 1 — Per‑voice signal path in POLY

**Status: 🔴 NOT STARTED** — Primary implementation target

Objective: move VCF + VCA to per‑voice processing for poly mode.

**Key Insight:** `Voice::vcf` and `Voice::env_filter` already exist and are initialized — they just aren't used in the POLY render loop!

Tasks:
1. ~~Add per‑voice VCF instance to voice structure~~ ✅ Already exists in `Voice::vcf`
2. ~~Add per‑voice VCA state~~ ✅ Already exists in `Voice::env_amp`
3. **Update poly render loop in drupiter_synth.cc:**
   - Location: [drupiter_synth.cc#L478-L595](../../drumlogue/drupiter-synth/drupiter_synth.cc)
   - For each active voice: DCO mix → (HPF if per-voice) → VCF → VCA
   - Apply `voice.env_filter` to VCF cutoff modulation
   - Apply `voice.vcf.Process()` instead of shared `vcf_.Process()`
4. Mix processed voice outputs into shared buffer with normalization.
5. Keep MONO/UNISON paths unchanged for now.

**Implementation Details:**

Current code (~line 580):
```cpp
float voice_output = voice_mix * voice_env;
mixed_ += voice_output;
// Later: mixed_ → shared HPF → shared VCF → out
```

Required change:
```cpp
// Process per-voice VCF envelope
const float voice_vcf_env = voice_mut.env_filter.Process();
float voice_cutoff_mod = cutoff_base * fast_pow2(voice_vcf_env * env_vcf_depth);

// Apply per-voice VCF
voice_mut.vcf.SetCutoffModulated(voice_cutoff_mod);
voice_mut.vcf.SetMode(vcf_mode);
float voice_filtered = voice_mut.vcf.Process(voice_mix);

// Apply per-voice VCA envelope
float voice_output = voice_filtered * voice_env;
mixed_ += voice_output;
// No more shared VCF needed for POLY
```

Validation:
- Audio parity test: poly tone and envelope behavior should match mono timing.
- CPU usage check: ensure poly mode remains within headroom (<80%).

Code changes to align:
- Add per‑voice `JupiterVcf vcf_` to voice struct and init/reset per note.
- Apply per‑voice VCF/VCA inside the poly voice loop before mixing.

Example (per‑voice filter chain):
// voice_allocator.h
struct Voice {
   JupiterVcf vcf_;
   // existing env_amp_, osc_, etc.
};

// drupiter_synth.cc (poly render loop)
for (Voice* v : active_voices_) {
   float left = v->osc_.Render();
   float right = left;
   v->vcf_.SetCutoff(cutoff_hz);
   v->vcf_.Process(&left, &right, 1);
   const float amp = v->env_amp_.Process();
   mix_l += left * amp;
   mix_r += right * amp;
}

## Phase 2 — HPF and filter order fidelity
Objective: match JP‑8 order and behavior.

Current implementation notes (drupiter):
- HPF is a simple one‑pole high‑pass applied globally after oscillator mix, before LPF; computed once per buffer (shared in all modes).
- LPF mode is selected via `vcf_type` and maps to 12 dB (2‑pole) or 24 dB (4‑pole) modes; resonance is handled in the shared VCF.
- Cutoff mapping is hybrid linear/exponential with a 100 Hz–12 kHz target range and modulation applied via `fast_pow2` of summed sources.
- In POLY, all voices are summed and then pass through the shared HPF/VCF/VCA (not per‑voice), so filter behavior is global.

Tasks:
1. Decide HPF placement in poly:
   - JP‑8 has per‑voice HPF; implement per‑voice HPF if feasible.
   - If CPU constrained, keep HPF shared but document deviation.
2. Verify filter slopes and resonance behavior vs JP‑8:
   - JP‑8 LPF is switchable 12 dB/oct (2‑pole) or 24 dB/oct (4‑pole), with a non‑resonant HPF in series (6 dB/oct). Confirm drupiter’s LPF mode mapping and HPF slope relative to these specs.
   - JP‑8 resonance is available at both LPF slopes; ensure resonance stability and scaling feel similar at 12/24 dB.
   - Map cutoff so that the subjective sweep range matches JP‑8 (low end remains audible, high end opens fully).
   - Current: cutoff mapping uses 100 Hz–12 kHz range with 15% linear blend; verify this matches JP‑8 sweep feel or adjust.
3. Update modulation depth scaling to match JP‑8 spec (ENV amount, LFO depth).

What we should do (Phase 2 deltas):
- Move HPF (and ideally VCF) into per‑voice path for POLY to align with JP‑8 per‑voice filtering.
- Confirm VCF mode mapping uses true 2‑pole/4‑pole responses and resonance scaling stays stable in both modes.
- Evaluate HPF slope: JP‑8 uses ~6 dB/oct non‑resonant; current implementation is a 1‑pole HPF (matches), but needs per‑voice placement.
- Re‑measure cutoff response curve vs JP‑8 and adjust mapping constants if needed.

Validation:
- Compare cutoff sweep response between mono and poly.
- Check resonance stability per voice.

Code changes to align:
- Move HPF into per‑voice chain (preferred) or document shared HPF as deviation.
- Ensure 12 dB mode taps after 2nd pole and 24 dB after 4th pole in `JupiterVcf`.
- Add resonance compensation (input boost) at high Q.

Example (HPF per‑voice):
// voice_allocator.h
struct Voice {
   OnePoleHpf hpf_;
   JupiterVcf vcf_;
};

// drupiter_synth.cc
v->hpf_.SetCutoff(hpf_hz);
v->hpf_.Process(&left, &right, 1);
v->vcf_.SetMode(vcf_mode); // 12dB/24dB
v->vcf_.Process(&left, &right, 1);

Example (VCF stage tap):
// jupiter_vcf.cc (pseudo)
float stage2 = ProcessTwoPole(x);
float stage4 = ProcessFourPole(x);
return (mode == kVcf12dB) ? stage2 : stage4;

## Phase 3 — Modulation routing alignment
Objective: ensure modulation sources are per‑voice in poly and match JP‑8.

Tasks:
1. LFO → VCO/VCF/VCA in poly:
   - JP‑8 uses a single LFO with selectable waveforms (triangle/saw/square/random) and delay (fade‑in). If global LFO is required, apply per‑voice with consistent phase, and implement delay behavior to match.
2. ENV → VCF and ENV → VCO pitch:
   - JP‑8 lets VCF select ENV‑1 or ENV‑2. Decide how drupiter maps ENV‑1/ENV‑2 to VCF, and ensure per‑voice application in poly.
3. Velocity and keyboard tracking:
   - JP‑8 VCF key follow supports 0–120%. Match this scaling, and apply per‑voice in poly where possible.
4. Confirm cross‑mod (XMOD) and sync behavior align with JP‑8 limitations.
5. VCA LFO modulation depth:
   - JP‑8 VCA LFO depth uses a discrete 4‑position switch. Decide whether drupiter should quantize to 4 steps or emulate a continuous control.

Validation:
- Modulation matrix tests in poly vs mono for parity.
- Compare envelopes and modulation depth at different velocities.

Code changes to align:
- Apply LFO/ENV modulation per voice, but keep a shared LFO phase if JP‑8 style is global.
- Add VCF ENV select (ENV‑1/ENV‑2) and VCA LFO depth switch (0–3).
- Add LFO delay (fade‑in) envelope shaping the modulation depth.

Example (LFO delay):
// drupiter_synth.cc (per block)
lfo_.Process(frames);
const float lfo_depth = lfo_delay_env_.Process();
const float lfo_value = lfo_.value() * lfo_depth;
v->vcf_.Modulate(lfo_value + env_value);

Example (VCA LFO depth switch):
// values 0..3 map to 0, 0.33, 0.66, 1.0
float vca_lfo = lfo_.value() * kVcaLfoDepths[vca_lfo_depth_];

## Phase 4 — Voice allocation behavior
Objective: match JP‑8 voice priority and allocation.

Tasks:
1. Implement JP‑8 style voice allocation (round‑robin with voice stealing).
2. Confirm envelope retrigger behavior in poly, mono, and unison.
3. Confirm unison detune stack matches JP‑8 unison behavior.

Validation:
- Play repeated chords; ensure deterministic voice steal behavior.
- Check release tails are preserved or stolen as expected.

Code changes to align:
- Implement round‑robin allocator with deterministic steal of oldest‑released voice.
- Match JP‑8 key priority when in mono/unison (highest or last note).

Example (round‑robin with release‑age):
// voice_allocator.cc (pseudo)
Voice* Allocate(uint8_t note) {
   Voice* v = FindFreeVoice();
   if (!v) v = FindOldestReleasedVoice();
   v->Start(note);
   return v;
}

## Phase 5 — Parameter calibration and UI parity
Objective: align knob ranges to JP‑8 musical response.

Tasks:
1. Calibrate DCO mix, detune, and PWM ranges.
2. Calibrate ENV attack/decay/release time curves (log/exp). 
3. Calibrate LFO rate and delay curves.
4. Review HPF/VCF cutoff scaling to match JP‑8 knob response.

Validation:
- Use test harness to compare envelope times across ranges.
- Sweep LFO and filter; verify expected response curve.

Code changes to align:
- Adjust mapping curves in `jupiter_env.cc` and `jupiter_vcf.cc` to match JP‑8 time/sweep ranges.
- Use exponential curves for decay/release and near‑linear for attack.

Example (ENV time mapping):
// jupiter_env.cc (pseudo)
float attack_s = kAttackMin + powf(knob, kAttackCurve) * (kAttackMax - kAttackMin);
float decay_s  = kDecayMin  + powf(knob, kDecayCurve)  * (kDecayMax - kDecayMin);

Example (VCF cutoff mapping):
// jupiter_vcf.cc (pseudo)
float cutoff_hz = kMinHz * powf(2.0f, knob * kOctaves);

## Phase 6 — Performance & stability
Objective: keep realtime constraints while adding per‑voice filters.

Tasks:
1. Optimize per‑voice filter computation (cache coeffs, avoid redundant recalcs).
2. Ensure no dynamic allocation in render.
3. Add optional perf counters to compare before/after.

Validation:
- CPU headroom test on hardware (target <80%).
- Long‑run stability test to detect denormals/NaN.

Code changes to align:
- Cache per‑voice filter coefficients and recompute only when params change.
- Use denormal flush in hot paths.

Example (coef caching):
// jupiter_vcf.cc
if (cutoff_ != new_cutoff || resonance_ != new_resonance) {
   ComputeCoefficients();
}

Example (denormal guard):
inline float FlushDenormal(float x) { return (fabsf(x) < 1e-15f) ? 0.0f : x; }

## Phase 7 — Hardware verification
Objective: verify behavior on drumlogue hardware.

Tasks:
1. Load updated unit, test mono, poly, unison.
2. Confirm no clipping or unexpected distortion.
3. Validate modulation and envelopes across voices.
4. Confirm preset save/load still works.

Deliverables:
- Updated unit build.
- Short release notes describing JP‑8 alignment changes.

Code changes to align:
- Add optional compile‑time perf stats (disabled by default) in `drumlogue/common/perf_mon.*`.
- Add debug counters for voice steals and clipping detection (desktop only).

Example (perf counter hook):
// drupiter_synth.cc
#if defined(PERF_MON)
perf_mon_.Begin();
// render...
perf_mon_.End();
#endif

## References (code)
- Main render loop: drumlogue/drupiter-synth/drupiter_synth.cc
- Voice allocation: drumlogue/drupiter-synth/dsp/voice_allocator.*
- DCO: drumlogue/drupiter-synth/dsp/jupiter_dco.*
- VCF: drumlogue/drupiter-synth/dsp/jupiter_vcf.*
- ENV: drumlogue/drupiter-synth/dsp/jupiter_env.*

## Optional research (if needed)
- Confirm JP‑8 voice architecture and modulation details from service manual.
- Confirm exact filter slope, HPF behavior, and resonance behavior.

## Research notes (filter/mod depth)
Findings to drive calibration steps:
- Filters: JP‑8 has a non‑resonant HPF (6 dB/oct) followed by a resonant LPF with selectable 12/24 dB slopes. Source: Roland technical specs and Patch & Tweak.
- VCF modulation source is selectable between ENV‑1 and ENV‑2; LFO modulation is also available. Source: Roland technical specs.
- LFO modulation and PWM have dedicated routing and source selection; LFO has delay (fade‑in) and multiple waveforms (triangle/saw/square/random). Source: Roland technical specs and Patch & Tweak.
- VCA LFO modulation depth is a discrete switch (0–3). Source: Roland technical specs.

Additional service‑manual notes (user‑provided):
- VCF IC: IR3109 (quad OTA). 24 dB mode uses all four stages; 12 dB mode likely uses two stages.
- Resonance calibration (VR14) targets stable self‑oscillation; model OTA feedback with mild non‑linear saturation (e.g., tanh) to emulate “growl.”
- Key follow range: 0–120% (can exceed 1V/oct tracking).
- Cutoff range calibration implies very high cutoff (periods down to ~5 µs → ~200 kHz), affecting perceived “openness.”
- HPF is separate, non‑resonant, before the main VCF.
- ENV IC: IR3R01 ADSR. Attack 1 ms–5 s; decay/release 1 ms–10 s; sustain 0–100%. RC‑like exponential decay/release; attack is often more linear than decay.
- Calibration reference: attack period set to ~6 s at specific settings (useful anchor for time‑constant fitting).
- VCA IC: BA662 OTA; exhibits saturation when driven.
- CV scaling: 1 V/oct (1/12 V per semitone).
- Note: some serial ranges report inverted VCF ENV MOD polarity.
- LFO depth: VCO modulation can reach ~2 octaves at maximum.

Links:
- https://support.roland.com/hc/en-us/articles/201945109-Jupiter-8-Technical-Specifications
- https://www.patchandtweak.com/jupiter-8-a-look-beneath-the-colorful-surface/

## Service Manual Technical Details (IR3109 Filter Architecture)

### IR3109 VCF Chip Specifications
From Roland service manual and technical analysis:

**Architecture:**
- Four OTA (Operational Transconductance Amplifier) stages integrated into single IR3109 chip
- Each OTA has associated buffer (stages 1/3: 0.6mA capacity, stages 2/4: 1mA and 1.3mA improved buffers)
- Exponential CV converter for 1V/oct tracking
- Wide transconductance range: 1µS to 10mS
- Low offset voltage: ±3mV typical
- High impedance MOS P-channel buffers

**JP-8 Filter Configuration:**
- **24 dB/oct mode**: All four OTA stages connected in series (standard 4-pole lowpass)
- **12 dB/oct mode**: Signal tapped after second OTA stage (2-pole lowpass)
- Output taken from stages 2 and 4 (the improved buffers)
- Q compensation circuit: Input signal boosted back into chip at high resonance to compensate for level reduction
- Self-oscillation capability with proper calibration (VR14 resonance trimmer)

**Filter Topology vs Other Roland Synths:**
- **Jupiter-4**: Simple 24dB implementation, no Q compensation, lower signal levels (noisy)
- **Jupiter-6**: Two 12dB state-variable filters (switchable LP/HP/BP modes)
- **Jupiter-8**: Optimized 2-pole/4-pole with Q compensation (production from 1980)
- **Juno-6/60**: Simplified JP-8 config (24dB only), added self-oscillation, retained Q compensation

**Implementation Notes:**
- Q compensation prevents volume drop at high resonance by feeding more input signal
- Anti-log CV circuitry provides exponential frequency response (1V/oct)
- Typical filter cutoff range extends to ~200kHz (5µs period) for "open" sound
- Resonance feedback can be modeled with tanh saturation for characteristic "growl"
- Some JP-8 serial ranges had inverted VCF ENV MOD polarity (firmware/hardware variation)

### IR3R01 Envelope Generator
**Timing Specifications:**
- Attack: 1ms to 5 seconds
- Decay: 1ms to 10 seconds
- Sustain: 0% to 100%
- Release: 1ms to 10 seconds

**Characteristics:**
- RC-based exponential curves for decay and release
- Attack tends to be more linear than decay/release
- Calibration reference: Attack period ~6 seconds at specific pot settings
- Two independent envelopes (ENV-1 for VCF/VCA, ENV-2 selectable for VCF)

### BA662 VCA
**Specifications:**
- OTA-based voltage-controlled amplifier
- Exhibits saturation characteristics when overdriven
- Discrete LFO modulation depth control (0/1/2/3 switch positions)
- Part of per-voice signal path in polyphonic architecture

### Control Voltage Standards
- **Pitch CV**: 1V/octave (1/12V = 1 semitone)
- **Filter tracking**: 0% to 120% (can exceed 1V/oct for over-tracking effects)
- **Gate voltage**: Off = 0V, On = 15V
- **CV out range**: 0-5V for highest note priority

### Additional Technical References
- Florian Anwander's Roland Filter Analysis: https://www.florian-anwander.de/roland_filters/
- Electric Druid IR3109 Design Analysis: https://electricdruid.net/roland-filter-designs-with-the-ir3109-or-as3109/

---

## 📊 Implementation Status Summary

| Phase | Description | Status | Priority | Key File |
|-------|-------------|--------|----------|----------|
| 0 | Baseline audit | ✅ Complete | - | - |
| 1 | Per-voice VCF in POLY | 🔴 Not started | **HIGH** | `drupiter_synth.cc` |
| 2 | Per-voice HPF | 🔴 Not started | Medium | `voice_allocator.h/cc` |
| 3 | Modulation routing | 🔴 Not started | Medium | `jupiter_lfo.h` |
| 4 | Voice allocation | 🟡 Verify | Low | `voice_allocator.cc` |
| 5 | Parameter calibration | 🔴 Not started | Low | `jupiter_env.h` |
| 6 | Performance & stability | 🔴 Not started | Medium | All |
| 7 | Hardware verification | 🔴 Not started | Final | - |

**Next Action:** Implement Phase 1 — the `Voice::vcf` and `Voice::env_filter` already exist, they just need to be used in the POLY render loop at [drupiter_synth.cc#L478-L595](../../drumlogue/drupiter-synth/drupiter_synth.cc).

---

## 📁 Files to Modify (Summary)

| File | Phases | Changes Required |
|------|--------|------------------|
| `drupiter_synth.cc` | 1, 2, 3, 5 | POLY render loop (use per-voice VCF/HPF), modulation routing |
| `voice_allocator.h` | 2 | Add `hpf_prev_output`, `hpf_prev_input` to Voice struct |
| `voice_allocator.cc` | 2, 4 | HPF state reset, verify voice stealing logic |
| `jupiter_env.h` | 5 | Adjust time constants to match JP-8 specs |
| `jupiter_lfo.h` | 3 | Add `key_trigger_` flag for note-reset |
| `jupiter_vcf.h/cc` | 5 | Verify cutoff range, Q compensation |

---

*Last updated: Code audit complete. Detailed implementation plan in [drupiter-jp8-detailed-implementation.md](drupiter-jp8-detailed-implementation.md)*