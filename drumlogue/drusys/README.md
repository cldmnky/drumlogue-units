# DRUSYS — Drumlogue System Diagnostic

Diagnostic unit that writes a comprehensive system report to `Programs/DRUSYS.TXT`.
Load once, retrieve the file via USB mass storage.

## Critical Findings

### Hardware

| Item | Detail |
|------|--------|
| **SoC** | NXP i.MX6 ULZ (Cortex-A7, 900 MHz, single-core) |
| **Board** | `imx6ull-14x14-evk` drumlogue Board |
| **RAM** | 256 MB DDR, ~180 MB free at idle, no swap |
| **Storage** | UBIFS on NAND flash, 4 MTD partitions: `2m(boot), 4m(kernel), 1m(dtb), -(data_app)` |
| **Audio codec** | PCM3168A (8ch) — ALSA `pcmC0D0p` (playback) + `pcmC0D1c` (capture) |
| **NEON** | `neon vfpv3 vfpv4 idiva idivt vfpd32 lpae` — full NEON with 32 VFP registers |
| **USB** | OTG `ci_hdrc.0` — USB mass storage + USB MIDI gadget |
| **I2C** | Bus 1, PMIC at `0x08` (`i2cget -y 1 0x08 0x00 b`) |
| **Thermal** | Not exposed in sysfs — no `/sys/class/thermal/` entries |

### Software

| Item | Detail |
|------|--------|
| **Kernel** | Linux 4.1.15-rt18 (PREEMPT_RT), built Jan 2023, GCC 6.5.0 crosstool-NG |
| **libc** | glibc 2.24, `GLIBCXX_3.4.21`, `CXXABI_1.3.9` |
| **Main process** | `drumlogued` (PID 102), 32 MB RSS, 4 threads |
| **Buffer** | 64 frames @ 48 kHz (1.33 ms) |
| **SDK API** | `0x00020000` |
| **Kernel config** | Available gzip-compressed at `/proc/config.gz` |

### Architecture

```
drumlogued (4 threads)
├── audio::AudioIO      — ALSA PCM playback (mmap), sample rate conversion via libsamplerate
├── audio::Unit          — dlopen/dlsym unit loader
├── core::VoiceManager   — STM32 voice co-processor (private UART /dev/ttymxc1)
├── core::MidiManager    — internal serial MIDI + USB MIDI (ALSA rawmidi hw:0,0)
├── core::PanelManager   — OLED/encoder UART (/dev/ttymxc2)
├── core::SequencerManager — step sequencer thread
├── core::SystemMonitor  — GPIO polling (power switch, headphones, sync-in), EUP auto-off
├── core::UIManager      — UI state machine
├── io::AudioFileProcessor — WAV sample conversion worker
└── data::Kit, Program, Chain, Globals, Sample
```

### Key Paths

| Path | Purpose |
|------|---------|
| `/var/lib/drumlogued/userfs/Programs/` | User SF2 files — **writable** from units |
| `/var/lib/drumlogued/userfs/Units/Synths/` | User synth units |
| `/var/lib/drumlogued/userfs/Units/{Delay,Reverb,Master}FXs/` | User FX units |
| `/usr/local/share/drumlogued/units/` | Factory units (synths/delfx/revfx/masterfx) |
| `/usr/local/share/drumlogued/samples/` | Factory drum samples (ch/cp/oh/rs/misc) |
| `/usr/local/share/drumlogued/kits/` | Factory kits |
| `/usr/local/share/drumlogued/programs/` | Factory programs |
| `/var/lib/drumlogued/globals` | Global settings (86 bytes) |
| `/var/lib/drumlogued/globals2` | Secondary global settings (122 bytes) |
| `/var/lib/drumlogued/chain` | Chain data |
| `/mnt/userfs.img` | User filesystem image (UBIFS mount) |
| `/usr/bin/bootarbiterd` | Boot arbiter daemon (134 KB) |

### Loaded Factory Units (hashes)

| Unit | Size | djb2 |
|------|------|------|
| Nano (synth) | 14 KB | `078091E2` |
| Compressor (masterfx) | 28 KB | `D723A39F` |
| Filter (masterfx) | 14 KB | `93772226` |
| Boost (masterfx) | 36 KB | `6F5F9A4D` |
| EQ Three (masterfx) | 41 KB | `8284FEFA` |
| Stereo (delfx) | 18 KB | `39025E63` |
| Mono (delfx) | 16 KB | `7A864D4E` |
| Tape (delfx) | 18 KB | `C8A03471` |
| Stereo BPM (delfx) | 18 KB | `631BC246` |
| Mono BPM (delfx) | 17 KB | `9B70F45C` |
| Tape BPM (delfx) | 19 KB | `E1D30060` |
| Room (revfx) | 22 KB | `87FF3287` |
| Hall (revfx) | 22 KB | `87FF3287` ⚠️ identical to Room |
| Space (revfx) | 24 KB | `1334D26B` |
| Riser (revfx) | 24 KB | `751738B2` |
| Submarine (revfx) | 25 KB | `F4A304AF` |

### File Format: KORGDLOG RIFF

All drumlogue data files use a proprietary **KORGDLOG RIFF** container format with CRC32
checksums (`sha1sum -c /checksums` runs at boot to verify). Magic bytes: `KORGDLOG`.

### Unit Loading

Units are ELF 32-bit shared objects loaded via `dlopen`/`dlsym`. The host searches:
1. Factory: `/usr/local/share/drumlogued/units/`
2. User: `/var/lib/drumlogued/userfs/Units/`

Unit callbacks: `unit_header`, `unit_init`, `unit_teardown`, `unit_reset`, `unit_resume`,
`unit_suspend`, `unit_render`, `unit_set_param_value`, `unit_get_param_value`,
`unit_get_param_str_value`, `unit_get_param_bmp_value`, `unit_note_on`, `unit_note_off`,
`unit_gate_on`, `unit_gate_off`, `unit_all_note_off`, `unit_pitch_bend`,
`unit_channel_pressure`, `unit_aftertouch`, `unit_set_tempo`,
`unit_load_preset`, `unit_get_preset_index`, `unit_get_preset_name`.

### Linked Libraries (available to all units)

| Library | Version | Size |
|---------|---------|------|
| libc | 2.24 | 1.1 MB |
| libm | 2.24 | 447 KB |
| libstdc++ | 6.0.22 | 996 KB |
| libpthread | 2.24 | 88 KB |
| librt | 2.24 | 26 KB |
| libdl | 2.24 | 9 KB |
| libgcc_s | 1 | 115 KB |
| libz | 1.2.11 | 101 KB |
| libsndfile | 1.0.28 | 387 KB |
| libsamplerate | 0.1.8 | 1.4 MB |
| libasound | 2.0.0 | 841 KB |

### Running Processes

- **PID 1**: `init` (BusyBox)
- **PID 59**: `rcS` — runs `/etc/init.d/rcS` at boot
- **PID 102**: `drumlogued` — main application (4 threads, 32 MB RSS)
- All other PIDs are kernel threads (interrupt handlers, workers, etc.)
- No network beyond loopback (`lo` only)

### Interesting Binaries on Device

| Binary | Purpose |
|--------|---------|
| `strings` | Extract printable strings |
| `md5sum`, `sha1sum`, `sha256sum`, `sha512sum` | File hashing |
| `ccrypt` | AES file encryption |
| `rsync` | File sync |
| `stm32prog` | STM32 firmware programmer |
| `i2cget`, `i2cset`, `i2cdump`, `i2cdetect` | I2C bus tools |
| `bootarbiterd` | Firmware update arbiter daemon |

### Factory Test Mode

`drumlogued --kensa=0|1|2|3` enables factory test mode:
- 0: Line tests (Inspector #1)
- 1: KIS tests (Inspector #2)
- 2: Shipping tests
- 3: Voice jig

Other test flags: `--test-audio-sine-all`, `--test-audio-pass`, `--test-audio-mix`,
`--disable-audio-mngr`, `--disable-midi-mngr`, `--disable-panel-mngr`, `--disable-voice-mngr`.

### Notes

- **No thermal monitoring** exposed — CPU temperature not available via sysfs
- **No swap** — all memory is physical RAM
- **Boot time**: ~2 minutes (59 seconds uptime at report time, kernel + userland init)
- **Single core** — all audio, panel, voice, MIDI, and unit processing shares one Cortex-A7
- **libsamplerate linked** — units can use `src_process()` for HQ resampling
- **libsndfile linked** — units can load/save WAV files from Programs/ at runtime
- **libz linked** — decompression available (though headers not in SDK container)
- **USB OTG state**: monitored at `/sys/class/udc/ci_hdrc.0/state`
- **RoHS compliance**: EUP (Energy Using Products) auto power-off via system monitor
