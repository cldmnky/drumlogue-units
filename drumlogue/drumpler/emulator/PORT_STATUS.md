# JV-880 Emulator Port - Status

## Status: ~90% Complete

### Completed
- Emulator sources integrated and building for drumlogue.
- LCD stubbed; JUCE/libresample dependencies removed.
- MCU MIDI queue converted to fixed-size array.
- Audio path wired: wrapper now calls `MCU::updateSC55WithSampleRate()` at 64kHz, then resamples to 48kHz.
- ROM-specific unit builds and links cleanly (no unexpected undefined symbols).
- Test harness added for desktop rendering.
- Parameters exposed (PART/POLY/LEVEL/PAN/TONE/CUTOFF/RESO/ATTACK/REVERB/CHORUS/DELAY).
- Preset selection wired via Program Change (TONE parameter and LoadPreset).

### In Progress
- Preset names parsed from ROM (currently uses numeric P### strings).
- MIDI/event timing: wrapper currently uses immediate `postMidiSC55()` (no sample-accurate queue).

### Remaining Work
1. **Preset names**
   - Parse ROM preset names and return in `getPresetName()` / parameter strings.
2. **Audio validation**
   - Run desktop test harness output through Audacity/sox.
   - Test on hardware (presets editor).
3. **Performance**
   - Measure CPU on hardware or QEMU; optimize hot paths if needed.

## Test Harness
Location: test/drumpler/

Example:
```
make -C test/drumpler
./test/drumpler/drumpler_test --rom drumlogue/drumpler/resources/jv880_base_srjv80-10.bin \
  --out fixtures/drumpler_test.wav --seconds 2.0 --note 60 --velocity 100 --program 0
```

## Notes
- ROM build script now generates external linkage for `g_drumpler_rom` symbols.
- The current audio path uses a simple linear resampler (64kHz -> 48kHz).
- Parameter layout from PORTING_PLAN.md is implemented with MIDI CC mappings.
