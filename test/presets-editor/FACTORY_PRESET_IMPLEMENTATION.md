# Factory Preset Implementation Summary

## Overview
Added support for displaying and loading unit-defined factory presets in the preset editor GUI.

## What Was Done

### 1. Unit Loader Updates (`core/unit_loader.{h,c}`)

**Added function pointers** to `unit_loader_t` struct:
```c
typedef struct {
    // ... existing fields ...
    
    // Optional: Preset loading (drumlogue FW ≥1.0.0)
    unit_load_preset_func unit_load_preset;
    unit_get_preset_name_func unit_get_preset_name;
} unit_loader_t;
```

**Symbol resolution** in `unit_loader_open()`:
```c
// Optional preset functions
loader->unit_load_preset = (unit_load_preset_func)dlsym(handle, "unit_load_preset");
loader->unit_get_preset_name = (unit_get_preset_name_func)dlsym(handle, "unit_get_preset_name");
```

### 2. GUI Updates (`gui/imgui_app.cpp`)

**Added "Factory Presets" section** in `render_presets_panel()`:
```cpp
// Factory presets from unit
if (hdr->num_presets > 0 && loader_->unit_load_preset && loader_->unit_get_preset_name) {
    ImGui::Separator();
    ImGui::Text("Factory Presets:");
    
    for (uint32_t i = 0; i < hdr->num_presets; ++i) {
        ImGui::PushID(1000 + i);  // Offset to avoid ID conflicts
        
        if (ImGui::Button("Load")) {
            // Load factory preset
            loader_->unit_load_preset(static_cast<uint8_t>(i));
            
            // Update param cache from unit
            for (uint32_t p = 0; p < hdr->num_params && p < param_values_.size(); ++p) {
                if (loader_->unit_get_param_value) {
                    param_values_[p] = loader_->unit_get_param_value(static_cast<uint8_t>(p));
                }
            }
        }
        ImGui::SameLine();
        
        const char* preset_name = loader_->unit_get_preset_name(static_cast<uint8_t>(i));
        ImGui::Text("%s", preset_name ? preset_name : "Factory Preset");
        
        ImGui::PopID();
    }
}
```

**UI Layout**:
- User presets listed first (JSON files from `presets/` directory)
- Separator line
- Factory presets section (if unit defines any)
- Each factory preset has a "Load" button

### 3. Documentation

Created:
- `FACTORY_PRESETS.md` - Detailed documentation on factory preset support
- Updated `README.md` - Added factory preset info to Phase 4 and new preset section
- `test-factory-presets.sh` - Test script to verify functionality

## How It Works

1. **Unit Header** defines `num_presets` (e.g., pepege-synth has 6)
2. **Unit Callbacks** implement:
   - `unit_get_preset_name(index)` - Returns preset name string
   - `unit_load_preset(index)` - Loads preset into unit state
3. **Loader** resolves these symbols during unit loading
4. **GUI** checks if presets exist and displays them
5. **User** clicks "Load" on a factory preset
6. **Callback** `unit_load_preset()` is called
7. **GUI** reads back parameter values via `unit_get_param_value()`
8. **UI** updates to show new parameter values

## Example: pepege-synth

pepege-synth defines 6 factory presets in `unit.cc`:
```cpp
__unit_callback void unit_load_preset(uint8_t idx) {
    s_synth.LoadPreset(idx);
}

__unit_callback const char* unit_get_preset_name(uint8_t idx) {
    return s_synth.GetPresetName(idx);
}
```

Header declares:
```c
.num_presets = 6,  // Built-in factory presets
```

## Testing

```bash
cd test/presets-editor

# Build GUI
make gui

# Test with pepege-synth
./bin/presets-editor-gui --unit units/pepege_synth.dylib
```

Expected behavior:
1. Unit loads successfully
2. Preset browser shows "Factory Presets" section
3. Six factory presets listed
4. Clicking "Load" updates all parameters to preset values

## Benefits

- **No JSON files needed** - Factory presets built into unit
- **Always available** - Ship with the unit
- **Starting points** - Users can load factory presets as templates
- **Demonstration** - Show recommended parameter combinations
- **Consistent** - Same presets as hardware

## Future Enhancements

- Export factory presets to JSON (create user preset from factory preset)
- Mark current factory preset in UI
- Preset comparison (A/B between factory and user presets)
- Hardware export (convert JSON to .drmlgpreset format)
