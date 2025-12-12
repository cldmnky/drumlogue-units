# QEMU ARM Testing Framework - Bug Fixes

This document summarizes the fixes applied to get the QEMU ARM testing framework working.

## Summary

Fixed **four cascading bugs** in the SDK stub implementation that prevented drumlogue units from loading and initializing in the QEMU ARM test environment.

## Bugs Fixed

### 1. Target Mismatch (sdk_stubs.c)
**Problem:** Static target value (0x0400) didn't match unit-specific targets (e.g., 0x0404 for revfx).

**Root Cause:** SDK stubs initialized with base drumlogue target but didn't account for module type bits.

**Fix:** Added `sdk_stubs_set_target()` function called after unit load to update target from unit header:
```c
void sdk_stubs_set_target(uint16_t target) {
    if (g_runtime_desc) {
        g_runtime_desc->target = target;
    }
}
```

**Impact:** Eliminated k_unit_err_target (-1) initialization failures.

---

### 2. unit_header_t Struct Alignment (sdk_stubs.h)
**Problem:** `target` field declared as `uint32_t` instead of `uint16_t`, causing 2-byte offset corruption.

**Evidence:** Unit name read as "ds Reverb" instead of "Clds Reverb", all subsequent fields garbled.

**Fix:** 
- Changed `target` from `uint32_t` to `uint16_t`
- Added `#pragma pack(1)` to enforce byte-aligned packing
```c
#pragma pack(push, 1)
typedef struct unit_header {
    // ...
    uint16_t target;      // Was: uint32_t
    // ...
} unit_header_t;
#pragma pack(pop)
```

**Impact:** Header now reads correctly from .drmlgunit binary.

---

### 3. unit_param_t Bitfield Packing (sdk_stubs.h)
**Problem:** Fields `frac` and `frac_mode` declared as separate `uint8_t` instead of packed bitfields.

**Evidence:** Parameter names showed gibberish, min/max values corrupted, array size wrong (char[16] vs char[14]).

**Fix:**
- Used C bitfield syntax: `frac:4`, `frac_mode:1`, `reserved:3`
- Corrected name array size to `char[14]`
- Added `#pragma pack(1)`
```c
#pragma pack(push, 1)
typedef struct unit_param {
    int32_t min;
    int32_t max;
    int32_t center;
    int32_t init;
    uint8_t type;
    uint8_t frac:4;         // Was: uint8_t frac
    uint8_t frac_mode:1;    // Was: uint8_t frac_mode
    uint8_t reserved:3;     // NEW
    uint16_t flags;
    char name[14];          // Was: char[16]
} unit_param_t;
#pragma pack(pop)
```

**Impact:** Parameter descriptors now parse correctly, parameter names readable.

---

### 4. API Version Constant (sdk_stubs.h)
**Problem:** `UNIT_API_VERSION` hardcoded as `0x01000000U` (v1.0.0) instead of `0x00020000U` (v2.0.0).

**Evidence:** unit_init() returned k_unit_err_api_version (-2), API compatibility check failed.

**Root Cause:** SDK v2.0.0 uses encoding `((major << 16) | (minor << 8) | sub)`, was using wrong value.

**Fix:**
```c
// Was: #define UNIT_API_VERSION 0x01000000U
#define UNIT_API_VERSION 0x00020000U  // k_unit_api_2_0_0 = ((2U << 16) | (0U << 8) | (0U))
```

**Impact:** Unit initialization now succeeds (return value 0).

---

## Build System Issue

**Problem:** After fixing API version in header, make didn't detect change (header-only modification).

**Workaround:** Deleted binary and object files manually to force rebuild:
```bash
rm -f unit_host_arm *.o
make -f Makefile.podman podman-build
```

**Future:** Consider adding `podman-clean` target to Makefile.podman.

---

## Validation

**Test Command:**
```bash
./run_qemu.sh /path/to/unit.drmlgunit input.wav output.wav --verbose
```

**Expected Output:**
```
✓ Loaded unit: /path/to/unit.drmlgunit
Unit Information:
  Name: Clds Reverb
  API: 0x00020000
  ...
✓ Unit initialized successfully
Processing audio...
✓ Processing complete: 1.00 seconds processed
```

**Success Criteria:**
- Unit loads without errors
- Name and metadata read correctly
- Parameters display with proper names
- unit_init() returns 0
- Audio processing completes

---

## Technical Details

### Struct Packing
ARM ABI may add padding between struct fields for alignment. Using `#pragma pack(1)` forces byte-aligned packing to match .drmlgunit binary layout exactly.

### API Version Encoding
SDK uses major/minor/sub packed into uint32_t:
- Bits 16-23: Major version
- Bits 8-15: Minor version
- Bits 0-7: Sub version

Example: v2.0.0 = (2 << 16) | (0 << 8) | (0) = 0x00020000

### UNIT_API_IS_COMPAT Macro
Compatibility check in SDK:
```c
#define UNIT_API_IS_COMPAT(x) \
    (((x) & 0x00FF0000U) == ((UNIT_API_VERSION) & 0x00FF0000U) && \
     ((x) & 0x0000FF00U) <= ((UNIT_API_VERSION) & 0x0000FF00U))
```
Requires exact major version match AND runtime minor <= unit minor.

---

## Files Modified

- `test/qemu-arm/sdk_stubs.h` - Fixed struct definitions and API version
- `test/qemu-arm/sdk_stubs.c` - Added sdk_stubs_set_target() function
- `test/qemu-arm/unit_host.c` - Call sdk_stubs_set_target() after unit load

---

## Cleanup Completed

Removed 8 unused files:
- `README.drums.md`
- `README.elementish.md`
- `Makefile.test` (3 copies)
- `README.qemu.md` (3 copies)

---

## Testing Workflow

1. **Build ARM binary:**
   ```bash
   make -f Makefile.podman podman-build
   ```

2. **Generate test signal:**
   ```python
   python3 -c "import struct, math; ..."
   ```

3. **Run in QEMU:**
   ```bash
   ./run_qemu.sh /path/to/unit.drmlgunit input.wav output.wav
   ```

4. **Verify output:**
   - Check output.wav exists
   - Listen to processed audio
   - Analyze with Audacity/SoX

---

## Date: 2024-12-12
## Status: ✅ RESOLVED
