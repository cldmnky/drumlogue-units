# Piano Roll Feature

## Overview
Added an interactive piano roll to the preset editor, allowing you to play notes on synth units by clicking keys.

## Features

- **Visual Piano Keyboard**: One octave (C-B) with white and black keys
- **Octave Shift**: +/- buttons to shift up/down by octaves (range: C2 to B6)
- **Click to Play**: Click and hold keys to trigger notes
- **Visual Feedback**: Active notes are highlighted in blue
- **Velocity**: Fixed at 100 for simplicity
- **Auto-Off**: All notes turn off when releasing mouse or stopping audio

## UI Layout

The piano roll appears at the bottom of the window **only for synth units** (not effects).

```
┌─ Piano Roll ──────────────────────────────┐
│ Octave: [-] +0 (C4-B4) [+]               │
│ ───────────────────────────────────────   │
│                                           │
│ [C][D][E][F][G][A][B]                    │
│ [#][#]   [#][#][#]   ← black keys        │
│                                           │
│ Click and hold keys to play notes        │
└───────────────────────────────────────────┘
```

## Usage

1. **Load a synth unit**:
   ```bash
   ./bin/presets-editor-gui --unit units/pepege_synth.dylib
   ```

2. **Start audio** (Audio menu → Start Audio)

3. **Play notes**:
   - Click a white or black key
   - Hold mouse button = note on
   - Release mouse button = note off

4. **Change octaves**:
   - Click `-` to go down an octave
   - Click `+` to go up an octave
   - Current range shown: e.g., "C4-B4"

## Technical Details

### Note Mapping
- Base octave: C4 (MIDI note 60)
- Octave offset: -2 to +3 (C2 to B6)
- Each key triggers `unit_note_on(note, velocity=100)`
- Release triggers `unit_note_off(note)`

### Synth Detection
The piano roll only appears when:
```cpp
(header->target & UNIT_TARGET_MODULE_MASK) == k_unit_module_synth
```

Effects units (delfx, revfx, masterfx) don't show the piano roll.

### Unit Callbacks
Uses standard SDK callbacks:
- `unit_note_on(uint8_t note, uint8_t velocity)` - Start note
- `unit_note_off(uint8_t note)` - Stop note
- `unit_all_note_off()` - Stop all notes (on audio shutdown)

### State Management
- 128-note array tracks active notes
- Notes automatically turn off on:
  - Mouse release
  - Audio stop
  - Window close

## Benefits

- **Interactive testing**: Play synths without MIDI controller
- **Parameter tweaking**: Adjust params while holding notes
- **Preset comparison**: Load preset, play notes, hear changes immediately
- **No external devices**: Built-in playability

## Limitations

- **Fixed velocity**: Always 100 (future: mouse Y position = velocity)
- **One octave visible**: Use +/- to change octaves
- **No sustain**: Note off on mouse release (future: keyboard shortcut for sustain)
- **Mouse-only**: No computer keyboard support yet

## Future Enhancements

- Computer keyboard mapping (A-K = C-B, etc.)
- Velocity control (click height or slider)
- Sustain pedal simulation (spacebar?)
- Multiple octaves visible
- MIDI input passthrough
- Record/playback sequences

## Example Workflow

1. Load pepege-synth
2. Start audio
3. Load factory preset "Bass"
4. Click and hold C4 key
5. Adjust CUTOFF parameter while holding note
6. Hear filter cutoff change in real-time
7. Release key
8. Try different notes to test tuning/octave params

Perfect for sound design and preset creation!
