# Proteus Analysis Tools

Scripts for analyzing EMU Proteus/1 patches and waveforms.

## Setup

Create a Python virtual environment with required dependencies:

```bash
cd drumlogue/druteus/tools
python3 -m venv .venv
.venv/bin/pip install numpy scipy
```

## Scripts

### analyze_proteus.py

Comprehensive analysis of Proteus patches and SF2 waveforms.

**Usage:**
```bash
# Basic analysis (prints to stdout)
.venv/bin/python3 analyze_proteus.py

# Export analysis to JSON
.venv/bin/python3 analyze_proteus.py --output analysis.json

# Export first 10 samples to WAV files
.venv/bin/python3 analyze_proteus.py --export-samples ./samples --num-samples 10

# Custom paths
.venv/bin/python3 analyze_proteus.py \
  --sf2 ../sfx/Proteus1_Instruments.sf2 \
  --patches proteus_patches.json
```

**Features:**
- SF2 file structure analysis (samples, instruments, presets)
- Sample statistics (duration, loop points, sample rates)
- Patch usage patterns (single/dual layer, LFO/envelope usage)
- Instrument popularity ranking
- Unused instrument detection
- Layer-specific usage analysis
- Parameter range statistics
- Sample export to WAV

**Output includes:**
- Total patches and layer distribution
- Most/least used instruments
- Crossfade mode usage
- LFO and envelope statistics
- Parameter ranges and distributions
- Sample-instrument mapping

### extract_patches.py

Extract patch data from .EMU SysEx files (already exists).

**Usage:**
```bash
# Generate JSON and C header (includes all patches)
python3 extract_patches.py \
  --dir ../tmp \
  --output proteus_patches.json \
  --cheader proteus_patches.h

# Filter out patches using Plus Orchestral expansion (recommended)
python3 extract_patches.py \
  --dir ../tmp \
  --output proteus_patches.json \
  --cheader proteus_patches.h \
  --skip-expansion
```

**Note:** Use `--skip-expansion` to filter out patches that reference Plus Orchestral expansion instruments (125-202). Since we only have the base Proteus/1 ROM samples in `Proteus1_Instruments.sf2`, these patches cannot be played and should be excluded. This reduces the patch count from 448 to 443.

### validate_instrument_map.py

Validate instrument-to-SF2 preset mapping (already exists).

**Usage:**
```bash
python3 validate_instrument_map.py
```

### verify_sf2.py

Comprehensive verification of SF2 completeness and mapping against the Proteus/1 manual.

**Usage:**
```bash
# Run verification (prints report to stdout)
.venv/bin/python3 verify_sf2.py

# Export report to JSON
.venv/bin/python3 verify_sf2.py --json report.json

# Custom paths
.venv/bin/python3 verify_sf2.py \
  --sf2 ../sfx/Proteus1_Instruments.sf2 \
  --patches proteus_patches.json
```

**Checks performed:**
- Instrument count matches manual (125 instruments)
- Instrument names match manual (with abbreviation tolerance)
- All samples are referenced by at least one instrument
- Keyboard coverage (no gaps in key ranges)
- Preset-to-instrument mapping validity
- Patch instrument IDs exist in SF2
- Sample type analysis

**See `VERIFICATION_REPORT.md` for the full verification results.**

## Analysis Results

### Key Findings (from 768 patches)

- **Layer usage:** 73.2% dual-layer, 26.8% single-layer
- **Most used instruments:**
  1. Oct 2 All (7.9%)
  2. Acoustic Guitar (6.6%)
  3. All Percussion (6.1%)
  4. Rock Bass (6.0%)
  5. Medium Envelope Pad (6.0%)

- **Unused instruments:** 19 instruments not used in any patches
- **LFO usage:** LFO1 in 30.9%, LFO2 in 25.4%
- **Envelope usage:** Env1 in 82.8%, Env2 in 83.2%
- **Crossfade:** 96.7% none, 3.3% X-Fade

### SF2 Statistics

- **Samples:** 263 total
- **Instruments:** 126
- **Presets:** 126
- **Total duration:** 112.72 seconds
- **Looped samples:** 99.6%
- **Sample rates:** 7kHz to 44kHz (varied)

## File Structure

```
drumlogue/druteus/tools/
├── .venv/                      # Python virtual environment
├── analyze_proteus.py          # Main analysis script
├── verify_sf2.py               # SF2 verification script
├── extract_patches.py          # SysEx extraction
├── validate_instrument_map.py  # Mapping validation
├── proteus_patches.json        # Extracted patch data
├── proteus_patches.h           # C header for firmware
├── proteus_instrument_map.h    # Instrument mapping
├── VERIFICATION_REPORT.md      # SF2 verification report
└── README.md                   # This file
```

## Requirements

- Python 3.8+
- numpy
- scipy (for WAV export)

## Notes

- The SF2 parser reads the file structure directly without external libraries
- Sample export requires scipy for WAV file writing
- Analysis assumes identity mapping between instrument IDs and SF2 presets
- Manual instrument names are from the Proteus/1 technical manual
