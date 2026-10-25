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
  and enum value tables, presets. No per-unit UI code.
- Plays from the on-screen keyboard, the computer keyboard (`z s x d c …`) and
  Web MIDI; records the output to a WAV file.
- **Benchmark** renders one second of audio as fast as possible and reports the
  DSP load (the emulator's own timing, not the browser's audio latency).

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
has them. The build can also run in a container, which is handy when the host
lacks Emscripten's dependencies:

```sh
podman run --rm --network=host \
  -v "$PWD:/src:Z" -v /path/to/logue-sdk:/sdk:Z -w /src/web \
  docker.io/emscripten/emsdk:latest make EMSCRIPTEN=/emsdk SDK=/sdk
```

## Deploying

`deploy/` has what OpenShift needs: a ConfigMap with the built site, a
Deployment running nginx-unprivileged, a Service and a Route. See
`deploy/README.md`.

## Limits

- Only units whose **sources** are available can be built; a binary
  `.drmlgunit` is ARM code and would need an emulator in the browser.
- Effect units need input audio; the shim exposes `host_in_ptr()` for it, but
  the UI does not route an input yet.
- No sample banks yet (`get_num_sample_banks` returns 0), so units that rely on
  the drumlogue's samples will be silent.
- The emulator is the unit's DSP, not the drumlogue: no sequencer, no global
  effects, no analog output stage.
