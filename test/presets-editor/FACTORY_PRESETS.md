# Factory Preset Support

The preset editor now displays factory presets that are built into drumlogue units.

## How It Works

Units can define built-in presets via:
- `num_presets` field in unit header (e.g., pepege-synth has 6)
- `unit_get_preset_name(index)` - Returns name of factory preset
- `unit_load_preset(index)` - Loads factory preset into unit state

## UI Integration

The preset browser panel now shows two sections:

### 1. Saved Presets
User-created presets stored as JSON files in `presets/` directory.
- **Save**: Enter name and click "Save Preset"
- **Load**: Click "Load" button next to preset name
- **Delete**: Click "Delete" button to remove preset file

### 2. Factory Presets
Built-in presets defined by the unit developer.
- **Load**: Click "Load" button next to factory preset name
- Read-only (cannot be modified or deleted)
- Parameter values are updated from unit after loading

## Example: pepege-synth

pepege-synth defines 6 factory presets:
1. Default wavetable sound
2. Bass preset
3. Lead preset
4. Pad preset
5. Pluck preset
6. FX preset

Load the unit and check the "Factory Presets" section in the preset browser to explore them.

## Testing

```bash
# Build the editor
cd test/presets-editor
make gui

# Run with pepege-synth
./bin/presets-editor-gui --unit units/pepege_synth.dylib

# In the GUI:
# 1. Open the preset browser panel
# 2. Scroll to "Factory Presets" section
# 3. Click "Load" on any factory preset
# 4. Parameters will update to preset values
```

## Implementation Details

**Unit Loader** (`core/unit_loader.c`):
- Resolves `unit_get_preset_name` and `unit_load_preset` symbols via dlsym
- Stores function pointers in `unit_loader_t` struct

**GUI** (`gui/imgui_app.cpp`):
- Checks `loader->header->num_presets > 0`
- Iterates through factory presets calling `unit_get_preset_name(i)`
- Displays each with a "Load" button
- On load: calls `unit_load_preset(i)`, then reads back parameter values

**Benefits**:
- No need to save factory presets as JSON files
- Always available with unit
- Can serve as starting points for custom presets
- Demonstrates parameter ranges and use cases
