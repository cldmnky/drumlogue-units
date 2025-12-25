# Drupiter Hoover Synthesis - Planning Document Index

**Status**: ✅ Planning Complete - Ready for Implementation  
**Total Documentation**: 5,040+ lines  
**Target Release**: v2.0.0  
**Timeline**: 5-8 weeks (4 phases)

---

## 📚 Planning Documents (Location: `/drumlogue/drupiter-synth/`)

### 1. 📘 PLAN-HOOVER.md (2,083 lines)
**Master technical specification document**

**Purpose**: Comprehensive architecture, design, and technical details  
**Audience**: Technical leads, architects, implementation team  
**Read Time**: 45-60 minutes

**Key Sections**:
- ✅ What is Hoover Synthesis (history & technology)
- ✅ Current State Analysis (existing Drupiter features)
- ✅ Missing Features (6 gaps, 4 critical for hoover)
- ✅ Polyphonic/Unison Architecture (detailed design)
- ✅ Synth Mode Architecture (monophonic, polyphonic, unison)
- ✅ Technical Specifications (VoiceAllocator, PolyVoice, detune algorithms)
- ✅ CPU/Memory Analysis (per-mode budgets, NEON optimization)
- ✅ Testing Strategy (desktop, QEMU ARM, hardware)
- ✅ Backward Compatibility Guarantee (parameter preservation)
- ✅ Phase Timeline (5-8 weeks breakdown)
- ✅ Risk Assessment & Mitigation

**When to Read**: First - for complete understanding of the implementation

---

### 2. 📗 SYNTHESIS_MODES.md (740 lines)
**Implementation guide and developer reference**

**Purpose**: Code-level architecture and implementation patterns  
**Audience**: Developers writing the code  
**Read Time**: 30-40 minutes

**Key Sections**:
- ✅ Quick Start: Build Commands (ready to use)
- ✅ Mode Overview (architecture, voice count, CPU, MIDI handling)
- ✅ Monophonic Mode (single voice, backward compatible)
- ✅ Polyphonic Mode (voice array, allocation strategies, render loops)
- ✅ Unison Mode (detuned oscillators, golden ratio phasing)
- ✅ Mode Configuration Matrix (build flags per mode)
- ✅ Mode Selection Guide (which mode for which use case)
- ✅ Building Multiple Variants (compile-time configuration)
- ✅ Preset Compatibility (guaranteed preservation)

**C++ Code Examples**:
- Voice struct definition
- PolyVoiceAllocator class interface
- Render loops for each mode
- Detune spread calculations
- MIDI routing logic

**When to Read**: Second - after PLAN-HOOVER.md, before coding

---

### 3. 📕 IMPLEMENTATION_ROADMAP.md (542 lines)
**Tactical execution plan with task breakdown**

**Purpose**: Task-by-task implementation roadmap with success criteria  
**Audience**: Project managers, task leads, developers  
**Read Time**: 20-30 minutes

**Key Sections**:
- ✅ Phase 1 Task Breakdown (5 subtasks with detailed deliverables)
  - Task 1.0: Voice Allocator (CRITICAL FOUNDATION)
  - Task 1.1: PWM Sawtooth Waveform
  - Task 1.2: Unison Oscillator
  - Task 1.3: Waveform Parameter Expansion
  - Task 1.4: MOD HUB Expansion
  - Task 1.5: Comprehensive Testing
- ✅ Per-Task Details (deliverables, code examples, testing, success criteria)
- ✅ Build System Integration (Makefile structure, CI/CD)
- ✅ Code Review Checklist
- ✅ Risk Mitigation Table
- ✅ Success Metrics (Phase 1 MVP completion)

**Phase 2-4 Overview**: Enhancements, Polish, Release

**When to Read**: Third - for task-level implementation planning

---

### 4. 📙 EXECUTIVE_SUMMARY.md (320 lines)
**High-level overview for stakeholders and decision-makers**

**Purpose**: Executive briefing on what's being built, why, and how  
**Audience**: Stakeholders, project sponsors, decision-makers  
**Read Time**: 10-15 minutes

**Key Sections**:
- ✅ What's Being Built (3 synthesis modes)
- ✅ Planning Documents Summary (brief overview)
- ✅ Key Architecture Decisions (4 major decisions)
- ✅ Feature Implementation Matrix (all 4 critical hoover features)
- ✅ Phase Timeline (5-8 weeks breakdown)
- ✅ CPU Budget Summary (per-mode targets)
- ✅ Risk Assessment (critical, medium risks & mitigation)
- ✅ Success Metrics (Phase 1 MVP & Final Release)
- ✅ Build Commands (ready to use)
- ✅ Next Steps (immediate actions)

**When to Read**: First - for quick overview before diving into details

---

## 📊 Document Relationships

```
EXECUTIVE_SUMMARY.md (10-15 min)
    ↓
    └─→ PLAN-HOOVER.md (45-60 min) [Complete technical details]
    └─→ SYNTHESIS_MODES.md (30-40 min) [Code-level implementation]
    └─→ IMPLEMENTATION_ROADMAP.md (20-30 min) [Task breakdown]
```

**Recommended Reading Order**:
1. Start: EXECUTIVE_SUMMARY.md (context & overview)
2. Deep Dive: PLAN-HOOVER.md (complete understanding)
3. Implementation: SYNTHESIS_MODES.md (coding patterns)
4. Execution: IMPLEMENTATION_ROADMAP.md (task planning)

---

## 📋 Planning Completeness Checklist

### Architecture Design ✅
- [x] Monophonic mode architecture (1 voice, v1.x compatible)
- [x] Polyphonic mode architecture (N independent voices)
- [x] Unison mode architecture (1 note + N detuned copies)
- [x] VoiceAllocator abstraction (foundational)
- [x] UnisonOscillator class design
- [x] Build system integration (compile-time mode selection)

### Feature Specification ✅
- [x] PWM Sawtooth waveform (Task 1.1)
- [x] Unison Oscillator (3-7 voices, Task 1.2)
- [x] Phase-Aligned Dual Output (Task 2.3, optional)
- [x] Pitch Envelope Modulation (Task 2.2)
- [x] MOD HUB Expansion (Task 1.4)
- [x] Waveform Parameter Extension (Task 1.3)

### Performance Analysis ✅
- [x] CPU budget per mode (20%, 65-75%, 50-65%)
- [x] Memory constraints (fixed buffers, no dynamic allocation)
- [x] NEON optimization strategy (2.5-3.5× speedup)
- [x] Worst-case performance scenarios

### Testing Strategy ✅
- [x] Desktop unit tests (spectrum, detune, CPU)
- [x] QEMU ARM profiling (realistic ARM performance)
- [x] Hardware validation (drumlogue)
- [x] Backward compatibility tests (all 6 factory presets)
- [x] Stress testing (10 minutes continuous)
- [x] Reference comparison (Alpha Juno)

### Backward Compatibility ✅
- [x] Parameter preservation guarantee
- [x] Waveform enum positioning (4+ for new waveforms)
- [x] Factory preset compatibility (6/6 unchanged)
- [x] MOD HUB expansion (non-breaking)
- [x] Preset migration strategy

### Phase Planning ✅
- [x] Phase 1: MVP (3-4 weeks)
  - [x] Task 1.0: Voice Allocator (critical foundation)
  - [x] Task 1.1: PWM Sawtooth
  - [x] Task 1.2: Unison Oscillator
  - [x] Task 1.3: Waveform Parameter
  - [x] Task 1.4: MOD HUB Expansion
  - [x] Task 1.5: Comprehensive Testing
- [x] Phase 2: Enhancements (2-3 weeks)
- [x] Phase 3: Polish (1-2 weeks)
- [x] Phase 4: Release (1 week)

### Risk Mitigation ✅
- [x] CPU budget exceeded → NEON optimization
- [x] Backward compatibility breaks → Preset testing matrix
- [x] Audio artifacts → PolyBLEP anti-aliasing, smoothing
- [x] Parameter conflicts → Careful mapping review

---

## 🎯 Key Features Verified

All 4 critical hoover synthesis features are specified and detailed:

| Feature | Status | Document | Task |
|---------|--------|----------|------|
| **PWM Sawtooth** | ✅ Specified | PLAN-HOOVER.md, IMPLEMENTATION_ROADMAP.md | 1.1 |
| **Unison (3-7 voices)** | ✅ Specified | PLAN-HOOVER.md, SYNTHESIS_MODES.md | 1.2 |
| **Phase-Aligned Output** | ✅ Optional | PLAN-HOOVER.md | 2.3 |
| **Pitch Envelope** | ✅ Specified | PLAN-HOOVER.md | 2.2 |

---

## 🏗️ Architecture Summary

### Three Synthesis Modes (Mutually Exclusive)

**Monophonic Mode:**
```
Single voice (v1.x compatible) → All 24 parameters → Single output
CPU: 20-25%
```

**Polyphonic Mode:**
```
N voices (2-6) → Each with pitch/velocity/envelope → Mixed output
Per-voice: OSC + Filter + Envelope
Shared: LFO
CPU: 40-90% (depends on voice count)
```

**Unison Mode:**
```
1 note + N detuned copies → Shared filter → Mixed stereo output
UnisonOscillator: 7 parallel oscillators with golden ratio detune
Shared: Filter, Envelope
CPU: 35-85% (depends on voice count)
```

### Voice Allocator (Foundation - Task 1.0)

Abstraction layer supporting all three modes:
- Monophonic path: Single voice state
- Polyphonic path: Voice array with round-robin allocation
- Unison path: Primary voice + UnisonOscillator

---

## 💾 File Structure

```
drumlogue/drupiter-synth/
├── PLAN-HOOVER.md (2,083 lines) ← Master spec
├── SYNTHESIS_MODES.md (740 lines) ← Developer guide
├── IMPLEMENTATION_ROADMAP.md (542 lines) ← Task breakdown
├── EXECUTIVE_SUMMARY.md (320 lines) ← Overview
├── DOCUMENT_INDEX.md (THIS FILE) ← Navigation
├── README.md (existing)
├── UI.md (existing)
├── RELEASE_NOTES.md (to be updated)
├── config.mk (to be modified)
├── header.c (to be modified)
├── unit.cc (to be modified)
├── drupiter_synth.h/cc (to be modified)
├── dsp/
│   ├── jupiter_dco.h/cc (to be modified - add SAW_PWM)
│   ├── voice_allocator.h/cc (NEW - Task 1.0)
│   └── unison_oscillator.h/cc (NEW - Task 1.2)
└── build/ (artifacts)
```

---

## 🚀 Quick Start for Implementation

### 1. Review Documents
```bash
# 1. Overview (10 min)
cat drumlogue/drupiter-synth/EXECUTIVE_SUMMARY.md

# 2. Complete Spec (45 min)
cat drumlogue/drupiter-synth/PLAN-HOOVER.md

# 3. Code Examples (30 min)
cat drumlogue/drupiter-synth/SYNTHESIS_MODES.md

# 4. Task Breakdown (20 min)
cat drumlogue/drupiter-synth/IMPLEMENTATION_ROADMAP.md
```

### 2. Build Test (Verify infrastructure)
```bash
# Monophonic (should work identical to v1.x)
cd /Users/mbengtss/code/src/github.com/cldmnky/drumlogue-units
make DRUPITER_MODE=MONOPHONIC clean build
./build.sh drupiter-synth
# Verify: drupiter-synth.drmlgunit created

# Polyphonic (compile-time configuration)
make DRUPITER_MODE=POLYPHONIC POLYPHONIC_VOICES=4 clean build
./build.sh drupiter-synth

# Unison (default 5-voice hoover)
make DRUPITER_MODE=UNISON UNISON_VOICES=5 clean build
./build.sh drupiter-synth
```

### 3. Begin Task 1.0
```bash
# Create voice allocator foundation
# See IMPLEMENTATION_ROADMAP.md → Task 1.0 for deliverables
# - Create dsp/voice_allocator.h (250-350 lines)
# - Create dsp/voice_allocator.cc (300-400 lines)
# - Modify drupiter_synth.h/cc to integrate allocator
# - Modify unit.cc to route MIDI through allocator
# - Verify monophonic fallback (backward compat test)
```

---

## 📞 Document Maintenance

**Last Updated**: 2025-12-25  
**Version**: 1.0 - Planning Complete  
**Status**: ✅ Ready for Implementation

**Future Updates**:
- Phase 2 additions (Tasks 2.1-2.4)
- Implementation notes during Task 1.0-1.5
- Performance profiling results
- Hardware validation findings
- Release notes (v2.0.0)

---

## ✅ Acceptance Criteria for Planning Phase

- [x] All 4 critical hoover features specified
- [x] Three synthesis modes (monophonic, polyphonic, unison) detailed
- [x] Voice Allocator abstraction designed
- [x] CPU budgets per mode established
- [x] Backward compatibility guaranteed and documented
- [x] Testing strategy comprehensive (desktop, ARM, hardware)
- [x] Phase timeline created (5-8 weeks)
- [x] Risk assessment complete with mitigations
- [x] Build system designed (compile-time mode selection)
- [x] All code examples provided
- [x] Success criteria defined (MVP, final)
- [x] Documentation complete and clear

**Status**: ✅ PLANNING PHASE COMPLETE
**Next Phase**: Implementation (Task 1.0 - Voice Allocator Foundation)

---

## 🔗 References & Links

**Related Documents in Repository**:
- `drumlogue/drupiter-synth/README.md` - Unit overview
- `drumlogue/drupiter-synth/UI.md` - Parameter and interface details
- `drumlogue/drupiter-synth/RELEASE_NOTES.md` - v1.x history

**External References**:
- Roland Alpha Juno documentation
- Korg Drumlogue SDK documentation
- ARM NEON SIMD optimization guides
- PolyBLEP anti-aliasing papers

**Copilot Instructions** (in `.github/`):
- `copilot-instructions.md` - Project structure and targets
- `instructions/cpp.instructions.md` - C/C++ coding standards
- `instructions/build-system.instructions.md` - Build guidelines
- `instructions/testing.instructions.md` - Testing standards
- `agents/drumlogue-dsp-expert.agent.md` - DSP expertise guide

---

**Ready to begin Phase 1 implementation?**  
See IMPLEMENTATION_ROADMAP.md → Task 1.0 for complete task breakdown.

