# UI Comparison: Factory Preset Support

## Before (User Presets Only)

```
┌─ Presets ─────────────────────────────┐
│                                        │
│  [ Preset Name     ]  [Save Preset]   │
│                                        │
│  ────────────────────────────────      │
│  Saved Presets:                        │
│                                        │
│  [Load] [Delete]  My Cool Sound        │
│  [Load] [Delete]  Bass Preset          │
│  [Load] [Delete]  Lead Sound           │
│                                        │
└────────────────────────────────────────┘
```

## After (User + Factory Presets)

```
┌─ Presets ─────────────────────────────┐
│                                        │
│  [ Preset Name     ]  [Save Preset]   │
│                                        │
│  ────────────────────────────────      │
│  Saved Presets:                        │
│                                        │
│  [Load] [Delete]  My Cool Sound        │
│  [Load] [Delete]  Bass Preset          │
│  [Load] [Delete]  Lead Sound           │
│                                        │
│  ────────────────────────────────      │
│  Factory Presets:                      │
│                                        │
│  [Load]  Default                       │
│  [Load]  Bass                          │
│  [Load]  Lead                          │
│  [Load]  Pad                           │
│  [Load]  Pluck                         │
│  [Load]  FX                            │
│                                        │
└────────────────────────────────────────┘
```

## Key Changes

1. **Separator line** between user and factory presets
2. **"Factory Presets:" section** added below user presets
3. **Load-only buttons** for factory presets (no Delete button)
4. **Preset names** from unit callbacks (e.g., "Default", "Bass", etc.)
5. **Automatic appearance** when `num_presets > 0` in unit header

## User Experience

**Before:**
- Only user-created JSON presets visible
- Need to manually create all presets from scratch
- Factory presets hidden (existed in unit but not accessible)

**After:**
- Both user and factory presets visible
- Factory presets serve as starting points
- Can load factory preset, tweak, and save as user preset
- Clear separation between read-only (factory) and editable (user) presets

## Example Workflow

1. Load unit (e.g., pepege-synth)
2. Browse factory presets in "Factory Presets" section
3. Click "Load" on "Bass" factory preset
4. Parameters update to bass sound settings
5. Tweak parameters (e.g., adjust filter cutoff)
6. Save as user preset: "My Bass v2"
7. Now have both:
   - Factory preset "Bass" (read-only, always available)
   - User preset "My Bass v2" (editable, can delete)
