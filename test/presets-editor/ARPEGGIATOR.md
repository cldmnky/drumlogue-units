# Arpeggiator Feature

## Overview
The preset editor now includes a simple arpeggiator with tempo control, allowing you to create rhythmic patterns from held notes.

## Features

### Controls
- **Arpeggiator Checkbox**: Enable/disable the arpeggiator
- **BPM Slider**: Control tempo from 40 to 240 BPM (default: 120 BPM)
- **Pattern Selector**: Choose arpeggio pattern:
  - **Up**: Notes play in ascending order
  - **Down**: Notes play in descending order
  - **Up-Down**: Notes play up then down (pingpong)
  - **Random**: Notes play in random order

### How It Works

**When Arpeggiator is OFF:**
- Click keys to play notes directly
- Hold mouse to sustain note
- Release mouse to stop note

**When Arpeggiator is ON:**
- Click keys to add them to the arpeggio
- Keys stay highlighted when clicked
- Release mouse to remove from arpeggio
- Arpeggiator automatically cycles through held notes at the set tempo
- 16th note resolution (4 steps per beat)

## Usage Examples

### Basic Arpeggio
1. Enable "Arpeggiator" checkbox
2. Set BPM to 120
3. Select "Up" pattern
4. Click C, E, and G keys
5. Listen to the arpeggiated C major chord

### Bass Line
1. Enable arpeggiator
2. Set BPM to 100
3. Select "Down" pattern
4. Click root note and octave above
5. Adjust filter while arp plays

### Ambient Texture
1. Enable arpeggiator
2. Set BPM to 60 (slow)
3. Select "Random" pattern
4. Click 4-5 notes across different octaves
5. Adjust reverb/effects while arp plays

## Technical Details

### Timing
- **Resolution**: 16th notes (4 steps per beat)
- **Tempo Range**: 40-240 BPM
- **Calculation**: `step_duration = 60 / (BPM × 4)`

### Pattern Algorithms

**Up:**
```
Step 0: Note 0
Step 1: Note 1
Step 2: Note 2
...
```

**Down:**
```
Step 0: Note N-1
Step 1: Note N-2
Step 2: Note N-3
...
```

**Up-Down:**
```
Cycle: 0, 1, 2, 1, 0, 1, 2, 1...
(Bounces at endpoints)
```

**Random:**
```
Each step: Random note from held notes
```

### Note Collection
- Notes are collected each frame based on which keys are "active"
- When arp is enabled, clicking a key toggles its active state
- Previous arp note is turned off before playing new note
- All notes turn off when arp is disabled or audio stops

## Integration with Other Features

### Works with:
- **Factory Presets**: Load preset, enable arp, test sound
- **Parameter Editing**: Adjust params while arp plays
- **Octave Shifting**: Change octave while arp is running
- **Audio Engine**: Arp continues as long as audio is running

### Status Display
When arpeggiator is active, the status line shows:
```
Arpeggiator active - 3 notes
```

When disabled:
```
Click and hold keys to play notes (velocity=100)
```

## Tips

1. **Building Patterns**: Start with 2-3 notes, then add more
2. **Tempo Sync**: Use BPM that matches your other gear (if any)
3. **Pattern Exploration**: Try different patterns on same notes
4. **Parameter Tweaks**: Adjust filter/envelope while arp plays
5. **Chord Progressions**: Change held notes to create progression

## Future Enhancements

- Gate length control (note duration vs silence)
- Swing/shuffle timing
- Multiple octave modes (play same note across octaves)
- Step sequencer mode (program specific patterns)
- Velocity per step
- MIDI clock sync (when MIDI input is added)
- Save/recall arp patterns with presets

## Example Workflow

1. Load pepege-synth
2. Start audio
3. Load "Bass" factory preset
4. Enable arpeggiator
5. Set BPM to 128
6. Select "Up" pattern
7. Click C3, E3, G3 keys
8. Adjust CUTOFF parameter to hear filter sweep
9. Change pattern to "Up-Down"
10. Add A3 to the arpeggio
11. Hear the extended pattern

Perfect for sound design, preset creation, and live jamming! 🎶
