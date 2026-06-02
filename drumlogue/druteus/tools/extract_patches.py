#!/usr/bin/env python3
"""
extract_patches.py — Parse EMU Proteus/1 .EMU SysEx files and output structured data.

SysEx format per preset (265 bytes), from Edisyn EmuProteus.java:
  [0]     F0  — SysEx start
  [1]     18  — E-Mu manufacturer ID
  [2]     04  — Proteus family product ID
  [3]     DevID
  [4]     01  — Command: preset dump
  [5]     preset# LSB  (preset# % 128)
  [6]     preset# MSB  (preset# / 128)
  [7..30] Name: 12 chars × 2 bytes (7-bit pair encoding)
  [31..262] Params: 116 params × 2 bytes (7-bit pair encoding)
  [263]   Checksum
  [264]   F7  — SysEx end

7-bit pair encoding:  value = data[offset] + data[offset+1] * 128
Signed values:        if value >= 8192: value -= 16384

Usage:
  python3 extract_patches.py [--dir <tmp-dir>] [--output <out.json>] [--cheader <out.h>]

Default tmp-dir: ../tmp  (relative to this script)
"""

import argparse
import json
import os
import struct
import sys
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# SysEx constants
# ---------------------------------------------------------------------------

SYSEX_START  = 0xF0
EMU_MFR_ID   = 0x18
PROTEUS_PID  = 0x04
CMD_DUMP     = 0x01
SYSEX_END    = 0xF7
PRESET_SIZE  = 265
NAME_PAIRS   = 12
PARAM_PAIRS  = 116    # indices 0..115

# ---------------------------------------------------------------------------
# Parameter definitions (from Edisyn EmuProteus.java, canonical order)
# Each entry: (param_index, name, min, max, signed)
# param_index 0..115 → SysEx bytes [31 + idx*2, 31 + idx*2 + 1]
# ---------------------------------------------------------------------------

PARAMS = [
    # index, name,                       min,  max,   signed
    (  0, "link1",                        -1,   511,  True  ),
    (  1, "link2",                        -1,   511,  True  ),
    (  2, "link3",                        -1,   511,  True  ),
    (  3, "lowkey0",                       0,   127,  False ),
    (  4, "lowkey1",                       0,   127,  False ),
    (  5, "lowkey2",                       0,   127,  False ),
    (  6, "lowkey3",                       0,   127,  False ),
    (  7, "highkey0",                      0,   127,  False ),
    (  8, "highkey1",                      0,   127,  False ),
    (  9, "highkey2",                      0,   127,  False ),
    ( 10, "highkey3",                      0,   127,  False ),
    # Primary instrument
    ( 11, "i1instrument",                  0, 16383,  False ),
    ( 12, "i1samplestartoffset",           0,   127,  False ),
    ( 13, "i1tuningcoarse",              -36,    36,  True  ),
    ( 14, "i1tuningfine",               -64,    64,  True  ),
    ( 15, "i1volume",                      0,   127,  False ),
    ( 16, "i1pan",                        -7,     7,  True  ),
    ( 17, "i1delay",                       0,   127,  False ),
    ( 18, "i1lowkey",                      0,   127,  False ),
    ( 19, "i1highkey",                     0,   127,  False ),
    ( 20, "i1attack",                      0,    99,  False ),
    ( 21, "i1hold",                        0,    99,  False ),
    ( 22, "i1decay",                       0,    99,  False ),
    ( 23, "i1sustain",                     0,    99,  False ),
    ( 24, "i1release",                     0,    99,  False ),
    ( 25, "i1envelopeon",                  0,     1,  False ),
    ( 26, "i1solomode",                    0,     1,  False ),
    ( 27, "i1chorus",                      0,    15,  False ),
    ( 28, "i1reversesound",                0,     1,  False ),
    # Secondary instrument
    ( 29, "i2instrument",                  0, 16383,  False ),
    ( 30, "i2samplestartoffset",           0,   127,  False ),
    ( 31, "i2tuningcoarse",              -36,    36,  True  ),
    ( 32, "i2tuningfine",               -64,    64,  True  ),
    ( 33, "i2volume",                      0,   127,  False ),
    ( 34, "i2pan",                        -7,     7,  True  ),
    ( 35, "i2delay",                       0,   127,  False ),
    ( 36, "i2lowkey",                      0,   127,  False ),
    ( 37, "i2highkey",                     0,   127,  False ),
    ( 38, "i2attack",                      0,    99,  False ),
    ( 39, "i2hold",                        0,    99,  False ),
    ( 40, "i2decay",                       0,    99,  False ),
    ( 41, "i2sustain",                     0,    99,  False ),
    ( 42, "i2release",                     0,    99,  False ),
    ( 43, "i2envelopeon",                  0,     1,  False ),
    ( 44, "i2solomode",                    0,     1,  False ),
    ( 45, "i2chorus",                      0,    15,  False ),
    ( 46, "i2reversesound",                0,     1,  False ),
    # Crossfade
    ( 47, "crossfademode",                 0,     2,  False ),
    ( 48, "crossfadedirection",            0,     1,  False ),
    ( 49, "crossfadebalance",              0,   127,  False ),
    ( 50, "crossfadeamount",               0,   255,  False ),
    ( 51, "switchpoint",                   0,   128,  False ),
    # LFO 1
    ( 52, "lfo1shape",                     0,     4,  False ),
    ( 53, "lfo1frequency",                 0,   127,  False ),
    ( 54, "lfo1delay",                     0,   127,  False ),
    ( 55, "lfo1variation",                 0,   127,  False ),
    ( 56, "lfo1amount",                 -128,   127,  True  ),
    # LFO 2
    ( 57, "lfo2shape",                     0,     4,  False ),
    ( 58, "lfo2frequency",                 0,   127,  False ),
    ( 59, "lfo2delay",                     0,   127,  False ),
    ( 60, "lfo2variation",                 0,   127,  False ),
    ( 61, "lfo2amount",                 -128,   127,  True  ),
    # Auxiliary Envelope
    ( 62, "i3delay",                       0,   127,  False ),
    ( 63, "i3attack",                      0,    99,  False ),
    ( 64, "i3hold",                        0,    99,  False ),
    ( 65, "i3decay",                       0,    99,  False ),
    ( 66, "i3sustain",                     0,    99,  False ),
    ( 67, "i3release",                     0,    99,  False ),
    ( 68, "i3amount",                   -128,   127,  True  ),
    # Key/Velocity modulation — 6 slots
    ( 69, "keyvelsource1",                 0,     1,  False ),
    ( 70, "keyvelsource2",                 0,     1,  False ),
    ( 71, "keyvelsource3",                 0,     1,  False ),
    ( 72, "keyvelsource4",                 0,     1,  False ),
    ( 73, "keyvelsource5",                 0,     1,  False ),
    ( 74, "keyvelsource6",                 0,     1,  False ),
    ( 75, "keyveldest1",                   0,    33,  False ),
    ( 76, "keyveldest2",                   0,    33,  False ),
    ( 77, "keyveldest3",                   0,    33,  False ),
    ( 78, "keyveldest4",                   0,    33,  False ),
    ( 79, "keyveldest5",                   0,    33,  False ),
    ( 80, "keyveldest6",                   0,    33,  False ),
    ( 81, "keyvelamount1",              -128,   127,  True  ),
    ( 82, "keyvelamount2",              -128,   127,  True  ),
    ( 83, "keyvelamount3",              -128,   127,  True  ),
    ( 84, "keyvelamount4",              -128,   127,  True  ),
    ( 85, "keyvelamount5",              -128,   127,  True  ),
    ( 86, "keyvelamount6",              -128,   127,  True  ),
    # Real-time modulation — 8 slots
    ( 87, "realtimesource1",               0,     9,  False ),
    ( 88, "realtimesource2",               0,     9,  False ),
    ( 89, "realtimesource3",               0,     9,  False ),
    ( 90, "realtimesource4",               0,     9,  False ),
    ( 91, "realtimesource5",               0,     9,  False ),
    ( 92, "realtimesource6",               0,     9,  False ),
    ( 93, "realtimesource7",               0,     9,  False ),
    ( 94, "realtimesource8",               0,     9,  False ),
    ( 95, "realtimedest1",                 0,    24,  False ),
    ( 96, "realtimedest2",                 0,    24,  False ),
    ( 97, "realtimedest3",                 0,    24,  False ),
    ( 98, "realtimedest4",                 0,    24,  False ),
    ( 99, "realtimedest5",                 0,    24,  False ),
    (100, "realtimedest6",                 0,    24,  False ),
    (101, "realtimedest7",                 0,    24,  False ),
    (102, "realtimedest8",                 0,    24,  False ),
    # Global / controllers
    (103, "footswitchdest1",               0,    10,  False ),
    (104, "footswitchdest2",               0,    10,  False ),
    (105, "footswitchdest3",               0,    10,  False ),
    (106, "controlleramount1",          -128,   127,  True  ),
    (107, "controlleramount2",          -128,   127,  True  ),
    (108, "controlleramount3",          -128,   127,  True  ),
    (109, "controlleramount4",          -128,   127,  True  ),
    (110, "pressureamount",             -128,   127,  True  ),
    (111, "pitchbendrange",                0,    13,  False ),
    (112, "velocitycurve",                 0,     5,  False ),
    (113, "keyboardcenter",                0,   128,  False ),
    (114, "submix",                        0,     2,  False ),
    (115, "keyboardtuning",                0,     5,  False ),
]

assert len(PARAMS) == PARAM_PAIRS, f"Expected {PARAM_PAIRS} params, got {len(PARAMS)}"

PARAM_NAMES = [p[1] for p in PARAMS]
PARAM_SIGNED = {p[1]: p[4] for p in PARAMS}

# ---------------------------------------------------------------------------
# Decoding helpers
# ---------------------------------------------------------------------------

def decode_pair(data: bytes, offset: int) -> int:
    """Decode a 14-bit unsigned value from a 7-bit LSB/MSB pair."""
    return data[offset] + data[offset + 1] * 128


def decode_pair_signed(data: bytes, offset: int) -> int:
    """Decode a 14-bit signed value (two's complement)."""
    v = decode_pair(data, offset)
    return v - 16384 if v >= 8192 else v


def decode_name(data: bytes) -> str:
    """Decode 12-char preset name from bytes 7..30."""
    chars = []
    for i in range(NAME_PAIRS):
        offset = 7 + i * 2
        c = decode_pair(data, offset)
        if 0x20 <= c <= 0x7E:
            chars.append(chr(c))
        else:
            chars.append(' ')
    return ''.join(chars).rstrip()


def decode_params(data: bytes) -> dict:
    """Decode all 116 parameters from bytes 31..262."""
    result = {}
    for idx, name, pmin, pmax, signed in PARAMS:
        offset = 31 + idx * 2
        if signed:
            v = decode_pair_signed(data, offset)
        else:
            v = decode_pair(data, offset)
        result[name] = v
    return result


def verify_checksum(data: bytes) -> bool:
    """Verify SysEx checksum.

    Sum of all name+param bytes (bytes 7..262, i.e. 128 pairs × 2 bytes),
    masked to 7 bits.  The preset# bytes [5,6] are NOT included.
    Confirmed by byte-probing the actual .EMU files.
    """
    payload_sum = sum(data[7:263]) & 0x7F
    return payload_sum == data[263]


def parse_preset(data: bytes, source_file: str, file_offset: int) -> Optional[dict]:
    """Parse a single 265-byte preset SysEx block."""
    if len(data) < PRESET_SIZE:
        return None
    if data[0] != SYSEX_START or data[1] != EMU_MFR_ID or data[2] != PROTEUS_PID:
        return None
    if data[4] != CMD_DUMP:
        return None
    if data[264] != SYSEX_END:
        return None

    preset_num = data[5] + data[6] * 128
    name = decode_name(data)
    params = decode_params(data)
    checksum_ok = verify_checksum(data)

    return {
        "preset_number": preset_num,
        "name": name,
        "source_file": os.path.basename(source_file),
        "file_offset": file_offset,
        "checksum_ok": checksum_ok,
        "params": params,
    }


# ---------------------------------------------------------------------------
# File scanning
# ---------------------------------------------------------------------------

def find_emu_files(base_dir: Path) -> list[Path]:
    """Find all .EMU files under base_dir, deduplicating by content."""
    found = []
    seen_hashes = set()
    for root, dirs, files in os.walk(base_dir):
        dirs.sort()
        for f in sorted(files):
            if f.upper().endswith('.EMU'):
                path = Path(root) / f
                content = path.read_bytes()
                h = hash(content)
                if h not in seen_hashes:
                    seen_hashes.add(h)
                    found.append(path)
    return found


def parse_emu_file(path: Path) -> list[dict]:
    """Parse all presets from a single .EMU file."""
    data = path.read_bytes()
    presets = []
    offset = 0
    while offset + PRESET_SIZE <= len(data):
        chunk = data[offset:offset + PRESET_SIZE]
        if chunk[0] == SYSEX_START:
            p = parse_preset(chunk, str(path), offset)
            if p is not None:
                presets.append(p)
                offset += PRESET_SIZE
                continue
        # Scan forward for next F0
        next_f0 = data.find(SYSEX_START, offset + 1)
        if next_f0 == -1:
            break
        offset = next_f0
    return presets


# ---------------------------------------------------------------------------
# C header generation
# ---------------------------------------------------------------------------

C_HEADER_TEMPLATE = """\
/**
 * @file proteus_patches.h
 * @brief Auto-generated EMU Proteus/1 patch data extracted from .EMU SysEx files.
 *
 * Generated by extract_patches.py from {num_files} .EMU files.
 * Total presets: {num_presets}
 *
 * DO NOT EDIT — regenerate with extract_patches.py
 */
#pragma once
#include <stdint.h>

/* Number of presets */
#define PROTEUS_PATCH_COUNT  {num_presets}

/* Preset name (12 chars + NUL) */
typedef struct {{
    char     name[13];
    uint16_t preset_number;
    int16_t  i1instrument;
    int16_t  i2instrument;
    int8_t   i1tuningcoarse;
    int8_t   i1tuningfine;
    int8_t   i2tuningcoarse;
    int8_t   i2tuningfine;
    uint8_t  i1volume;
    uint8_t  i2volume;
    int8_t   i1pan;
    int8_t   i2pan;
    uint8_t  i1chorus;
    uint8_t  i2chorus;
    uint8_t  crossfademode;
    uint8_t  lfo1shape;
    uint8_t  lfo2shape;
    /* —– Phase 3C: key ranges & crossfade —– */
    uint8_t  i1lowkey;
    uint8_t  i1highkey;
    uint8_t  i2lowkey;
    uint8_t  i2highkey;
    uint8_t  switchpoint;
    uint8_t  crossfadedirection;
    uint8_t  crossfadebalance;
    uint8_t  crossfadeamount;
    /* —– Phase 3D: per-preset LFO —– */
    uint8_t  lfo1frequency;
    uint8_t  lfo1delay;
    uint8_t  lfo1variation;
    int8_t   lfo1amount;
    uint8_t  lfo2frequency;
    uint8_t  lfo2delay;
    uint8_t  lfo2variation;
    int8_t   lfo2amount;
    /* —– Phase 3E: per-layer envelopes —– */
    uint8_t  i1attack;
    uint8_t  i1hold;
    uint8_t  i1decay;
    uint8_t  i1sustain;
    uint8_t  i1release;
    uint8_t  i1envelopeon;
    uint8_t  i2attack;
    uint8_t  i2hold;
    uint8_t  i2decay;
    uint8_t  i2sustain;
    uint8_t  i2release;
    uint8_t  i2envelopeon;
    /* —– Phase 4A: per-layer delay & solo —– */
    uint8_t  i1delay;
    uint8_t  i2delay;
    uint8_t  i1solomode;
    uint8_t  i2solomode;
    /* —– Phase 4B: sample offset & reverse —– */
    uint8_t  i1samplestartoffset;
    uint8_t  i2samplestartoffset;
    uint8_t  i1reversesound;
    uint8_t  i2reversesound;
    /* —– Phase 4C: auxiliary envelope —– */
    uint8_t  i3delay;
    uint8_t  i3attack;
    uint8_t  i3hold;
    uint8_t  i3decay;
    uint8_t  i3sustain;
    uint8_t  i3release;
    int8_t   i3amount;
    /* —– Phase 4D: pitch bend range —– */
    uint8_t  pitchbendrange;
}} proteus_patch_t;

/* Preset table */
static const proteus_patch_t kProteusPatchTable[PROTEUS_PATCH_COUNT] = {{
{rows}
}};

/* Get preset name by index (0-based into the table above) */
static inline const char *proteus_patch_name(int idx) {{
    if (idx < 0 || idx >= PROTEUS_PATCH_COUNT) return "";
    return kProteusPatchTable[idx].name;
}}
"""


def format_c_string(s: str, maxlen: int = 12) -> str:
    """Escape a string for a C string literal."""
    s = s[:maxlen]
    escaped = s.replace('\\', '\\\\').replace('"', '\\"')
    return f'"{escaped}"'


def generate_c_header(presets: list[dict], source_files: list[str]) -> str:
    rows = []
    for p in presets:
        pr = p["params"]
        name = p["name"][:12]
        row = (
            f'    {{ {format_c_string(name, 12)}, '
            f'{p["preset_number"]}, '
            f'{pr["i1instrument"]}, '
            f'{pr["i2instrument"]}, '
            f'{pr["i1tuningcoarse"]}, '
            f'{pr["i1tuningfine"]}, '
            f'{pr["i2tuningcoarse"]}, '
            f'{pr["i2tuningfine"]}, '
            f'{pr["i1volume"]}, '
            f'{pr["i2volume"]}, '
            f'{pr["i1pan"]}, '
            f'{pr["i2pan"]}, '
            f'{pr["i1chorus"]}, '
            f'{pr["i2chorus"]}, '
            f'{pr["crossfademode"]}, '
            f'{pr["lfo1shape"]}, '
            f'{pr["lfo2shape"]}, '
            f'{pr["i1lowkey"]}, '
            f'{pr["i1highkey"]}, '
            f'{pr["i2lowkey"]}, '
            f'{pr["i2highkey"]}, '
            f'{pr["switchpoint"]}, '
            f'{pr["crossfadedirection"]}, '
            f'{pr["crossfadebalance"]}, '
            f'{pr["crossfadeamount"]}, '
            f'{pr["lfo1frequency"]}, '
            f'{pr["lfo1delay"]}, '
            f'{pr["lfo1variation"]}, '
            f'{pr["lfo1amount"]}, '
            f'{pr["lfo2frequency"]}, '
            f'{pr["lfo2delay"]}, '
            f'{pr["lfo2variation"]}, '
            f'{pr["lfo2amount"]}, '
            f'{pr["i1attack"]}, '
            f'{pr["i1hold"]}, '
            f'{pr["i1decay"]}, '
            f'{pr["i1sustain"]}, '
            f'{pr["i1release"]}, '
            f'{pr["i1envelopeon"]}, '
            f'{pr["i2attack"]}, '
            f'{pr["i2hold"]}, '
            f'{pr["i2decay"]}, '
            f'{pr["i2sustain"]}, '
            f'{pr["i2release"]}, '
            f'{pr["i2envelopeon"]}, '
            f'{pr["i1delay"]}, '
            f'{pr["i2delay"]}, '
            f'{pr["i1solomode"]}, '
            f'{pr["i2solomode"]}, '
            f'{pr["i1samplestartoffset"]}, '
            f'{pr["i2samplestartoffset"]}, '
            f'{pr["i1reversesound"]}, '
            f'{pr["i2reversesound"]}, '
            f'{pr["i3delay"]}, '
            f'{pr["i3attack"]}, '
            f'{pr["i3hold"]}, '
            f'{pr["i3decay"]}, '
            f'{pr["i3sustain"]}, '
            f'{pr["i3release"]}, '
            f'{pr["i3amount"]}, '
            f'{pr["pitchbendrange"]} '
            f'}},  /* {p["source_file"]} */'
        )
        rows.append(row)
    return C_HEADER_TEMPLATE.format(
        num_files=len(source_files),
        num_presets=len(presets),
        rows='\n'.join(rows),
    )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Parse EMU Proteus/1 .EMU SysEx files")
    script_dir = Path(__file__).parent
    default_tmp = script_dir.parent / "tmp"
    parser.add_argument("--dir", type=Path, default=default_tmp,
                        help=f"Directory to scan for .EMU files (default: {default_tmp})")
    parser.add_argument("--output", type=Path, default=None,
                        help="JSON output file (default: stdout)")
    parser.add_argument("--cheader", type=Path, default=None,
                        help="C header output file")
    parser.add_argument("--verbose", action="store_true",
                        help="Print each preset name as it is parsed")
    args = parser.parse_args()

    emu_files = find_emu_files(args.dir)
    if not emu_files:
        print(f"No .EMU files found under {args.dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Found {len(emu_files)} .EMU files:", file=sys.stderr)
    for f in emu_files:
        print(f"  {f}", file=sys.stderr)

    all_presets = []
    bad_checksums = 0
    for path in emu_files:
        presets = parse_emu_file(path)
        print(f"  {path.name}: {len(presets)} presets", file=sys.stderr)
        for p in presets:
            if not p["checksum_ok"]:
                bad_checksums += 1
                print(f"    WARNING: bad checksum for preset {p['preset_number']} '{p['name']}'",
                      file=sys.stderr)
            if args.verbose:
                print(f"  [{p['preset_number']:3d}] {p['name']!r:14s} "
                      f"i1={p['params']['i1instrument']:3d} "
                      f"i2={p['params']['i2instrument']:3d} "
                      f"vol={p['params']['i1volume']:3d}/{p['params']['i2volume']:3d}",
                      file=sys.stderr)
        all_presets.extend(presets)

    print(f"\nTotal: {len(all_presets)} presets "
          f"({bad_checksums} bad checksums)", file=sys.stderr)

    # Summary: unique preset names
    unique_names = sorted({p["name"] for p in all_presets})
    print(f"Unique preset names: {len(unique_names)}", file=sys.stderr)

    # JSON output
    output_data = {
        "source_files": [str(f) for f in emu_files],
        "total_presets": len(all_presets),
        "bad_checksums": bad_checksums,
        "param_names": PARAM_NAMES,
        "presets": all_presets,
    }
    if args.output:
        args.output.write_text(json.dumps(output_data, indent=2))
        print(f"JSON written to {args.output}", file=sys.stderr)
    else:
        # Print summary table to stdout (not full JSON)
        for p in all_presets:
            pr = p["params"]
            print(f'{p["source_file"]:20s} [{p["preset_number"]:3d}] {p["name"]!r:14s} '
                  f'i1={pr["i1instrument"]:5d} i2={pr["i2instrument"]:5d} '
                  f'vol={pr["i1volume"]:3d}/{pr["i2volume"]:3d} '
                  f'pan={pr["i1pan"]:+d}/{pr["i2pan"]:+d} '
                  f'xfade={pr["crossfademode"]}')

    # C header output
    if args.cheader:
        header_text = generate_c_header(all_presets, [str(f) for f in emu_files])
        args.cheader.write_text(header_text)
        print(f"C header written to {args.cheader}", file=sys.stderr)


if __name__ == "__main__":
    main()
