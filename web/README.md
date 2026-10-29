# drumlogue web emulator

Runs a drumlogue unit's DSP in the browser: the unit is compiled to WebAssembly,
an `AudioWorklet` calls its `unit_render()` in the drumlogue's own 64-frame
blocks, and the UI is built from the unit's `unit_header` - so any unit can be
played without writing a front end for it.

```
web/
  Makefile            builds a unit's sources to www/units/<unit>.wasm
  src/unit_shim.c     host side of the unit API, exports host_* to JS
  www/                the page: index.html, app.js, worklet.js, styles.css
  www/units/          built units (git-ignored) + index.json
```

## What it does

- Loads a unit compiled to WebAssembly, initialises it at the AudioContext's
  sample rate with 64-frame buffers and renders in an `AudioWorklet`.
- Builds the controls from the unit's header: parameter names, ranges, string
  and enum value tables, presets, and the drumlogue's own page grouping (four
  parameters at a time, with an "All" view). No per-unit UI code.
- Plays from the on-screen keyboard, the computer keyboard (`z s x d c …`) and
  Web MIDI; records the output to a WAV file.
- Effect units get their input from the **in** selector: `silence` or a 220 Hz
  `tone` (the unit's module type decides whether it gets an input at all, and
  effects start on the tone so they are audible).
- **Benchmark** renders one second of audio as fast as possible and reports the
  DSP load. It runs on the audio thread, so the output pauses while it runs -
  it is a measurement, not something to hold a note through.

Units that build today: `pepege-synth`, `elementish-synth`, `drupiter-synth`
(synths) and `clouds-revfx` (reverb, needs the input tone). `druteus` needs a
change to its own sources (`simd_utils.h` is NEON-only) and `drusys` is a
diagnostic unit that renders silence and imports syscall stubs, so neither is
built here.

## Building

Needs an activated [emsdk](https://emscripten.org). The one that ships with the
logue SDK works too:

```sh
cd logue-sdk/tools/emsdk
./emsdk install latest && ./emsdk activate latest
```

Then, from `web/`:

```sh
make                                  # pepege-synth.wasm
make UNIT=clouds-revfx                # any unit under drumlogue/
make EMSCRIPTEN=/path/to/emsdk        # if emsdk is not ~/emsdk
make SDK=/path/to/logue-sdk           # if the SDK is not at ../logue-sdk
make serve                            # http://localhost:8000/
```

A worktree of the repo has no submodules, so point `SDK=` at a checkout that
has them. Units that reuse Mutable Instruments DSP (`clouds-revfx`, ...) need
the `eurorack` submodule too; the build looks for it in `../eurorack` and can
be pointed elsewhere with `EURORACK=`.

The build can also run in a container, which is handy when the host lacks
Emscripten's dependencies:

```sh
podman run --rm --network=host \
  -v "$PWD:/src:Z" -v /path/to/logue-sdk:/sdk:Z \
  -v /path/to/eurorack:/src/eurorack:Z \
  -w /src/web \
  docker.io/emscripten/emsdk:latest make EMSCRIPTEN=/emsdk SDK=/sdk UNIT=clouds-revfx
```

## Deploying

`deploy/` has what OpenShift needs: a ConfigMap with the built site, a
Deployment running a small static server (`python -m http.server`) as a
non-root user, a Service and an edge-TLS Route. `make deploy` builds the
ConfigMap from `www/` and applies it; see `deploy/README.md`.

## Limits

- Only units whose **sources** are available can be built; a binary
  `.drmlgunit` is ARM code and would need an emulator in the browser.
- Effect units need input audio; the shim exposes `host_in_ptr()` for it, but
  the UI does not route an input yet (its "tone" mode writes the shim's input
  buffer, which effect units ignore while the runtime descriptor reports zero
  input channels).
- `drusys` is a diagnostic unit, not a synth: it writes a report to the unit
  filesystem and renders silence. It also imports two Emscripten syscalls
  (`env.__syscall_getdents64`, `env.__syscall_uname`) that the AudioWorklet's
  import object does not provide, so it does not instantiate in the page.
- No sample banks yet (`get_num_sample_banks` returns 0), so units that rely on
  the drumlogue's samples will be silent.
- The emulator is the unit's DSP, not the drumlogue: no sequencer, no global
  effects, no analog output stage.
