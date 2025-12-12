# QEMU ARM Testing - Quick Reference

## One-Line Test

```bash
./test-unit.sh clouds-revfx
```

## What Gets Tested

✅ Unit loads correctly  
✅ API version compatibility  
✅ Parameter descriptors  
✅ Unit initialization  
✅ Audio processing (ARM code paths)  
✅ Output generation  

## Files

### Use These
- **`test-unit.sh`** - Simple test script (recommended)
- **`README.md`** - Full documentation
- **`generate_signals.py`** - Generate test WAV files

### Auto-Generated
- **`fixtures/*.wav`** - Test input signals
- **`build/*.wav`** - Test output files
- **`unit_host_arm`** - Compiled ARM binary

### Under the Hood
- **`unit_host.c`** - Unit loading/execution
- **`sdk_stubs.c/h`** - SDK runtime stubs
- **`wav_file.c/h`** - Audio I/O
- **`Makefile*`** - Build system

## Common Commands

```bash
# Test a specific unit
./test-unit.sh clouds-revfx

# Test all units
for u in elementish-synth pepege-synth drupiter-synth; do
  ./test-unit.sh $u
done

# Rebuild ARM binary
make -f Makefile.podman podman-build

# Generate test signals
python3 generate_signals.py

# Clean up
rm -rf build/*.wav *.o
```

## Output Files

- **`build/output_<unit>.wav`** - Processed audio
- Located in `build/` to keep root clean

## Troubleshooting

**Unit not found?**
```bash
cd ../.. && ./build.sh <unit-name>
```

**No test signals?**
```bash
python3 generate_signals.py
```

**Rebuild needed?**
```bash
rm unit_host_arm *.o
make -f Makefile.podman podman-build
```

## See Also

- [README.md](README.md) - Full documentation
- [FIXES.md](FIXES.md) - Bug fixes and technical details
