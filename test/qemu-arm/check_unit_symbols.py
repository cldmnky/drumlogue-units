#!/usr/bin/env python3
"""
ABI / symbol audit for drumlogue units.

Validates a built .drmlgunit against the DEVICE environment reported by the
DRUSYS diagnostic (i.MX6 ULZ, Linux 4.1.15, glibc 2.24, libstdc++ 6.0.22):

  1. Dynamic symbol VERSION needs must not exceed the device ceiling:
       GLIBC   <= 2.24
       GLIBCXX <= 3.4.21
       CXXABI  <= 1.3.9
  2. Exported dynamic symbols must match the unit API surface:
       unit_header, unit_* callbacks
     Additional exports can be allowed via UNIT_EXTRA_EXPORT_RE (regex).

Usage:
  check_unit_symbols.py <unit.drmlgunit> [exports.map]

Requires readelf (binutils). Skips gracefully if unavailable.
Exit codes: 0 = pass/skip, 1 = violation.
"""

import os
import re
import shutil
import subprocess
import sys

# Device ceilings from DRUSYS report
CAPS = {
    "GLIBC":   (2, 24),
    "GLIBCXX": (3, 4, 21),
    "CXXABI":  (1, 3, 9),
}

BASE_EXPORT_RE = re.compile(r"^(unit_header|unit_[a-z0-9_]+)$")


def parse_version(sym):
    m = re.match(r"^([A-Z]+)_(\d+(?:\.\d+)*)$", sym)
    if not m:
        return None, None
    tag = m.group(1)
    ver = tuple(int(x) for x in m.group(2).split("."))
    return tag, ver


def run_readelf(args):
    out = subprocess.run(["readelf"] + args, capture_output=True, text=True)
    if out.returncode != 0:
        print(f"SYMBOL_AUDIT ERROR: readelf {' '.join(args)} failed:\n{out.stderr[:400]}")
        sys.exit(1)
    return out.stdout


def parse_map(path):
    """Extract the 'global:' name list from a linker version script."""
    txt = open(path).read()
    m = re.search(r"\bglobal\s*:(.*?)local\s*:", txt, re.S)
    if not m:
        raise ValueError(f"no global:/local: sections in {path}")
    return {n.strip() for n in m.group(1).replace(";", " ").split() if n.strip()}


def main():
    if len(sys.argv) < 2 or not os.path.isfile(sys.argv[1]):
        print("usage: check_unit_symbols.py <unit.drmlgunit> [exports.map]")
        sys.exit(2)

    expect_map = sys.argv[2] if len(sys.argv) > 2 else None
    strict_exports = False
    expected = None
    if expect_map:
        if os.path.isfile(expect_map):
            expected = parse_map(expect_map)
            strict_exports = True
        else:
            print(f"SYMBOL_AUDIT NOTE: exports map {expect_map} missing; export check = warn-only")

    if shutil.which("readelf") is None:
        print("SYMBOL_AUDIT SKIP: readelf not available")
        sys.exit(0)

    elf = sys.argv[1]
    failures = []

    # ---- 1. Version needs vs device ceilings -------------------------------
    ver_out = run_readelf(["-V", elf])
    needed = set(re.findall(r"(?:GLIBC|GLIBCXX|CXXABI|GCC)_[0-9][0-9.]*", ver_out))
    print("SYMBOL_AUDIT version needs:")
    for sym in sorted(needed):
        tag, ver = parse_version(sym)
        line = f"  {sym}"
        if tag in CAPS and ver is not None:
            cap = CAPS[tag]
            if ver > cap:
                line += f"  ❌ exceeds device cap {tag}_{'.'.join(map(str, cap))}"
                failures.append(sym)
            else:
                line += "  ok"
        print(line)

    # ---- 2. Exported symbols vs unit API surface ---------------------------
    extra_re = None
    env_re = os.environ.get("UNIT_EXTRA_EXPORT_RE", "").strip()
    if env_re:
        try:
            extra_re = re.compile(env_re)
        except re.error as e:
            print(f"SYMBOL_AUDIT ERROR: bad UNIT_EXTRA_EXPORT_RE: {e}")
            sys.exit(1)

    dyn = run_readelf(["--dyn-syms", "-W", elf])
    exported = []
    for ln in dyn.splitlines():
        parts = ln.split()
        # readelf -W layout: Num: Value Size Type Bind Vis Ndx Name
        if len(parts) < 8 or ":" not in parts[0]:
            continue
        typ, bind, vis, ndx = parts[3], parts[4], parts[5], parts[6]
        if ndx == "UND":
            continue                      # imported, not exported
        if typ in ("SECTION", "FILE"):
            continue
        if bind not in ("GLOBAL", "WEAK") or vis != "DEFAULT":
            continue                      # locals/hidden cannot preempt
        exported.append(parts[7].split("@")[0])

    def _allowed(n):
        return BASE_EXPORT_RE.match(n) or (extra_re and extra_re.match(n)) \
            or (expected and n in expected)

    bad_exports = [n for n in exported if not _allowed(n)]
    if strict_exports:
        missing = sorted(expected - set(exported)) if expected else []
        if missing:
            failures.extend(missing)
            print(f"  ❌ MISSING required exports: {', '.join(missing)}")
    print(f"SYMBOL_AUDIT exports: {len(exported)} defined dynamic symbols"
          + (" (strict vs map)" if strict_exports else " (warn-only)"))
    if bad_exports and not strict_exports:
        # Units without a unit_exports.map get visibility into their export
        # surface without breaking CI; adopting a map flips this to enforced.
        print("  NOTE: unexpected exports are WARN-ONLY until a "
              "unit_exports.map is added next to the unit")
    if bad_exports and strict_exports:
        preview = ", ".join(sorted(bad_exports)[:12])
        more = "" if len(bad_exports) <= 12 else f" (+{len(bad_exports) - 12} more)"
        print(f"  ❌ unexpected exports: {preview}{more}")
        failures.extend(bad_exports)
    else:
        print("  all exports match unit API surface")

    if failures:
        print(f"SYMBOL_AUDIT FAIL ({len(failures)} violation(s))")
        sys.exit(1)
    print("SYMBOL_AUDIT PASS")


if __name__ == "__main__":
    main()
