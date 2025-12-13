# Computer Keyboard Input

The piano roll supports playing notes using your computer keyboard, making it easy to test synths without clicking.

## Key Mappings

### White Keys (C to C)
- `A` = C
- `S` = D  
- `D` = E
- `F` = F
- `G` = G
- `H` = A
- `J` = B
- `K` = C (next octave)

### Black Keys (Sharps)
- `W` = C#
- `E` = D#
- `T` = F#
- `Y` = G#
- `U` = A#

### Octave Control
- `Z` = Octave down (-1)
- `X` = Octave up (+1)

The current octave is displayed in the piano roll panel (e.g., "Octave: +0 (C4-B4)").

## Usage

### Direct Playing (Arpeggiator Disabled)
When arpeggiator is **off**:
- Press and hold keys to play notes
- Notes trigger immediately with velocity 100
- Release keys to stop notes

### Arpeggiator Mode (Arpeggiator Enabled)
When arpeggiator is **on**:
- Press keys to add notes to the arpeggiator pattern
- Notes don't trigger immediately
- The arpeggiator cycles through held notes at the specified tempo and pattern

### Hold Mode
When **Hold** checkbox is enabled:
- Notes latch - they stay in the pattern after you release the keys
- Press more keys to add additional notes
- Disable arpeggiator to clear all held notes

## Examples

### Play a C Major Chord
1. Disable arpeggiator
2. Hold `A`, `D`, `G` simultaneously
3. Result: C-E-G chord plays

### Quick Melody
1. Disable arpeggiator  
2. Press `A`, `S`, `D`, `F`, `G` in sequence
3. Result: C-D-E-F-G scale

### Arpeggio Pattern
1. Enable arpeggiator, set BPM to 120, Pattern to Up
2. Hold `A`, `D`, `G`
3. Result: Ascending C-E-G arpeggio

### Experiment Across Octaves
1. Press `Z` to go down an octave
2. Play some notes (lower pitched)
3. Press `X` `X` to go up two octaves  
4. Play the same notes (higher pitched)

## Tips

- **Combine mouse and keyboard**: Click piano roll keys and type on keyboard simultaneously
- **Use octave controls**: Quickly access wider note ranges
- **Test with presets**: Load presets and play melodies to hear how they sound
- **Arpeggiator + keyboard**: Build complex patterns by pressing multiple keys in hold mode
