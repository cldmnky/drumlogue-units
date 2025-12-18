# QEMU Testing Quick Reference

## Local Testing (Before Submitting PR)

Test your unit locally to catch issues before CI:

```bash
# 1. Build your unit
cd /path/to/drumlogue-units
./build.sh <unit-name>

# 2. Run QEMU test
cd test/qemu-arm
./test-unit.sh <unit-name>
```

**Example:**
```bash
./build.sh elementish-synth
cd test/qemu-arm
./test-unit.sh elementish-synth
```

## Test All Units

```bash
cd test/qemu-arm
for unit in clouds-revfx elementish-synth pepege-synth drupiter-synth; do
  echo "Testing $unit..."
  ./test-unit.sh "$unit" || echo "❌ $unit failed"
done
```

## CI/CD Pipeline

When you create a PR, the following happens automatically:

### 1. Build Stage
- All units are built using `./build.sh` in parallel
- Each `.drmlgunit` artifact is saved

### 2. QEMU Test Stage
- For each unit:
  - ARM host is built (`unit_host_arm`)
  - Test signals are generated
  - Unit is tested in QEMU ARM emulation
  - Output WAV files are saved as artifacts

### 3. Results
- ✅ Green check: All tests passed
- ❌ Red X: One or more units failed
- Click "Details" to see which unit failed and download output WAV files

## Viewing Test Results

1. Go to your PR on GitHub
2. Scroll to "Checks" section at the bottom
3. Click on "QEMU ARM Test / QEMU ARM Test (<unit-name>)"
4. View logs and download artifacts:
   - Build logs show compilation output
   - Test logs show QEMU execution
   - Artifacts contain output WAV files (available for 7 days)

## Debugging Failed Tests

### Build Failure
```
❌ Build ${{ matrix.unit }}
```

**Common causes:**
- Syntax errors in C/C++ code
- Missing includes or source files in config.mk
- Undefined symbols (check with `objdump -T`)

**Fix:** Build locally first: `./build.sh <unit>`

### QEMU Test Failure
```
❌ Run QEMU test for ${{ matrix.unit }}
```

**Common causes:**
- Unit initialization failed (API version mismatch)
- Crash during audio processing (NULL pointer, buffer overflow)
- Numerical issues (NaN, inf, denormals)

**Debug locally:**
```bash
cd test/qemu-arm
./test-unit.sh <unit-name> --verbose
# Check output WAV
open build/output_<unit-name>.wav
```

### Download Artifacts
Even if tests fail, you can download the output WAV to analyze the issue:

1. Go to failed workflow run
2. Scroll to bottom → "Artifacts"
3. Download `qemu-output-<unit-name>.zip`
4. Listen to/analyze the WAV file

## Performance Profiling

Test with profiling enabled:

```bash
./test-unit.sh <unit-name> --profile
```

This shows:
- CPU cycles per sample
- Function-level profiling (if available)
- Performance bottlenecks

## Troubleshooting

**"Unit file not found"**
→ Build first: `cd ../.. && ./build.sh <unit-name>`

**"ARM host not found"**  
→ Run: `make -f Makefile.podman podman-build`

**"Podman not found" (CI)**
→ Workflow installs podman automatically

**"Test signals not found"**
→ Run: `python3 generate_signals.py`

## Workflow Customization

The workflow file is at `.github/workflows/qemu-test.yml`.

**Add more test signals:**
Edit `generate_signals.py` to create additional WAV files.

**Test specific parameters:**
Modify `test-unit.sh` to pass parameter flags.

**Add new units:**
Units are auto-detected from the matrix:
```yaml
matrix:
  unit: [clouds-revfx, elementish-synth, pepege-synth, drupiter-synth]
```

Add new units to this list after creating them in `drumlogue/`.

## Best Practices

1. **Test locally before pushing** - Catch issues early
2. **Check all units** - Ensure changes don't break other units
3. **Review artifacts** - Listen to output WAV files
4. **Fix warnings** - Build warnings can indicate issues
5. **Use profiling** - Optimize hot paths

## Related Documentation

- [README.md](README.md) - Detailed QEMU testing guide
- [FIXES.md](FIXES.md) - SDK stub implementation details
- [PROFILING.md](PROFILING.md) - CPU profiling guide
- [../../.github/workflows/qemu-test.yml](../../.github/workflows/qemu-test.yml) - CI workflow source
