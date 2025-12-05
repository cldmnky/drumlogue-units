# Copilot Instructions for this Repo

- **Purpose & layout**: This repo combines (a) Mutable Instruments eurorack module sources (`eurorack/…`, STM32/AVR) and (b) Korg logue SDK materials (`logue-sdk/…`) for building drumlogue user units.
- **Main targets**: Most day-to-day work here is drumlogue unit development under `logue-sdk/platform/drumlogue/` using the provided templates (`dummy-synth`, `dummy-delfx`, `dummy-revfx`, `dummy-masterfx`).
- **Build tooling**: Always build through the Docker helper scripts inside `logue-sdk/docker/`.
  - Interactive shell (preferred during dev): `docker/run_interactive.sh` then `build drumlogue/dummy-synth` (or other path). Use `build -l --drumlogue` to list projects.
  - One-off CI-style: `./run_cmd.sh build drumlogue/dummy-synth` from repo root.
  - Clean: `build --clean <project>` or `./run_cmd.sh build --clean <project>`.
  - Final artifact: `<project>.drmlgunit` in the project dir (optionally also `.hex/.bin/.dmp/.list`).
- **Environment reset**: After interactive builds, the scripts auto-run `env -r` to reset. Keep this flow; don’t hand-roll compilers.
- **Project config**: Each drumlogue project uses `config.mk` to declare `PROJECT`, `PROJECT_TYPE` (`synth|delfx|revfx|masterfx`), `CSRC`, `CXXSRC`, include/library paths and extra `UDEFS`. Avoid editing the shared Makefiles directly.
- **Metadata header**: `header.c` is required and must stay aligned with the target module (`k_unit_module_*`), developer ID, unit ID, version encoding, and 7-bit ASCII name (<=13 chars). Parameters: up to 24 descriptors; blank slots use `{0,0,0,0,k_unit_param_type_none,...}` to control paging.
- **Unit API**: Implement unit callbacks in `unit.cc` (`unit_init`, `unit_render`, `unit_set_param_value`, preset getters/loaders, tempo, MIDI handlers). Drumlogue uses 48kHz float buffers; check `frames_per_buffer`, `input_channels`, `output_channels` from `unit_runtime_desc_t`.
- **Samples API**: Use runtime callbacks (`get_num_sample_banks`, `get_num_samples_for_bank`, `get_sample`) for sample access; `sample_wrapper_t` provides `frames`, `channels`, `sample_ptr`.
- **UI strings/bitmaps**: `k_unit_param_type_strings` and `k_unit_param_type_bitmaps` must return short 7-bit ASCII strings or 16x16 1bpp bitmaps; bitmap API needs drumlogue FW ≥1.1.0.
- **Developer IDs**: Choose a unique 32-bit `dev_id`; reserved: `0x00000000`, `0x4B4F5247`, `0x6B6F7267`. See `logue-sdk/developer_ids.md`.
- **Mutable Instruments code**: `eurorack/` contains upstream modules (e.g., `braids`). Resource generation scripts live in `resources/` and run during make to emit lookup tables (`resources.cc/h`). DSP sources are designed to compile both on hardware and desktop for testing (`test/` emits WAVs). Bootloader code sits in `bootloader/`.
- **Licensing**: Eurorack code is GPLv3 (AVR) / MIT (STM32); hardware CC-BY-SA. Respect the “Mutable Instruments” trademark notice in `eurorack/README.md` when producing derivatives.
- **Naming/ASCII**: Keep unit names and parameter strings to the allowed ASCII set noted in drumlogue docs; avoid non-ASCII in code unless already present.
- **When adding new units**: Copy a drumlogue template directory, rename, adjust `config.mk`, `header.c`, and `unit.cc`. Keep source lists minimal and prefer adding includes/libs via config vars rather than editing shared build scripts.
- **Debugging guidance**: For quick desktop-side checks in Mutable code, run the module’s `test/` harness to generate WAVs; for drumlogue, rely on the Docker build logs and load `.drmlgunit` onto hardware via USB mass storage.
- **External tools**: `tools/logue-cli` is legacy for other logue devices; for drumlogue, prefer USB mass storage (copy `.drmlgunit` to the device’s Units/* folder). Use logue-cli only if you know you need MIDI/SysEx for other platforms.

If any of these points feel incomplete or you need platform-specific build examples, tell me which unit or platform you’re touching and I’ll tighten the guidance.
