---
agent: 'agent'
model: Auto
tools: ['codebase', 'edit', 'runCommands', 'search']
description: 'Create a new drumlogue unit from template'
---

# Create New Drumlogue Unit

You are creating a new drumlogue unit. Ask for the following information if not provided:

1. **Unit name** (e.g., `reverb-fx`, `analog-synth`)
2. **Unit type**: synth, delfx (delay effects), revfx (reverb effects), or masterfx (master effects)
3. **Developer ID** (32-bit hex, or suggest generating a unique one)
4. **Unit ID** (0-127, or suggest next available)
5. **Short description** (for unit name, max 13 ASCII chars)

## Implementation Steps

1. **Create unit directory**: `drumlogue/<unit-name>/`

2. **Copy template files** from `logue-sdk/platform/drumlogue/dummy-<type>/`:
   - `Makefile` (copy as-is)
   - Template `config.mk`, `header.c`, `unit.cc`

3. **Configure `config.mk`**:
   ```makefile
   PROJECT := <unit_name>
   PROJECT_TYPE := <synth|delfx|revfx|masterfx>
   
   # Sources
   CSRC = 
   CXXSRC = unit.cc
   
   # Includes (if using common utilities)
   UINCDIR = 
   COMMON_INC_PATH = /workspace/drumlogue/common
   COMMON_SRC_PATH = /workspace/drumlogue/common
   
   # Libraries
   ULIBDIR = 
   ULIBS = 
   
   # Defines
   UDEFS = 
   ```

4. **Write `header.c`** with metadata:
   - Set `dev_id`, `unit_id`
   - Set unit name (7-bit ASCII, ≤13 chars)
   - Define parameters (up to 24)
   - Use blank slots with `k_unit_param_type_none` for paging

5. **Implement `unit.cc`**:
   - `unit_init()`: Initialize DSP state
   - `unit_render()`: Process audio buffers
   - `unit_set_param_value()`: Handle parameter changes
   - Preset functions (if needed)

6. **Build the unit**:
   ```bash
   ./build.sh <unit-name>
   ```

7. **Create test harness** (optional but recommended):
   - `test/<unit-name>/Makefile`
   - `test/<unit-name>/main.cc`
   - Implement WAV file processing for offline testing

## Best Practices

- Start with minimal DSP (pass-through or simple oscillator)
- Build and test incrementally
- Use desktop test harness for rapid iteration
- Follow existing units as reference (clouds-revfx, elementish-synth)
- Keep unit name ASCII-only and concise
- Document parameters clearly in header.c

## Example: Creating a Simple Delay Effect

```bash
# 1. Create directory
mkdir -p drumlogue/simple-delay

# 2. Template basis: dummy-delfx
# 3. Build
./build.sh simple-delay
```

After setup, implement DSP algorithm in `unit.cc::unit_render()` function.
