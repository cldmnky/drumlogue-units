// Runs a drumlogue unit's WebAssembly module: the WASI imports a standalone
// Emscripten reactor build asks for, the host_* API, and the unit's manifest.
//
// Loaded as a plain script by the page (for the main-thread renderer) and
// concatenated in front of worklet.js for the AudioWorklet, which cannot import
// modules - so keep this file dependency-free and es5-ish.
(function (global) {
  'use strict';

  // --- WASI env -------------------------------------------------------------
  function makeWasi() {
    var memory = null;
    var view = function () { return new DataView(memory.buffer); };
    var writeU32 = function (ptr, value) { view().setUint32(ptr, value, true); };
    var writeU64 = function (ptr, value) { view().setBigUint64(ptr, BigInt(value), true); };

    var imports = {
      wasi_snapshot_preview1: {
        proc_exit: function () { throw new Error('unit called proc_exit'); },
        fd_write: function () { return 0; },
        fd_close: function () { return 0; },
        fd_read: function () { return 0; },
        fd_seek: function () { return 0; },
        fd_fdstat_get: function () { return 0; },
        fd_fdstat_set_flags: function () { return 0; },
        fd_prestat_get: function () { return 8; },
        fd_prestat_dir_name: function () { return 8; },
        environ_get: function () { return 0; },
        environ_sizes_get: function (countPtr, sizePtr) {
          writeU32(countPtr, 0);
          writeU32(sizePtr, 0);
          return 0;
        },
        clock_time_get: function (id, precision, outPtr) {
          writeU64(outPtr, BigInt(Date.now()) * 1000000n);
          return 0;
        },
        clock_res_get: function (id, outPtr) { writeU64(outPtr, 1000n); return 0; },
        random_get: function (ptr, length) {
          var bytes = new Uint8Array(memory.buffer, ptr, length);
          for (var i = 0; i < length; i++) bytes[i] = (Math.random() * 256) | 0;
          return 0;
        },
        sched_yield: function () { return 0; },
        poll_oneoff: function () { return 0; },
      },
      env: {
        emscripten_notify_memory_growth: function () {},
        emscripten_memcpy_big: function (dest, src, num) {
          new Uint8Array(memory.buffer).copyWithin(dest, src, src + num);
          return dest;
        },
        __syscall_getdents64: function () { return 0; },
        __syscall_uname: function (buffer) {
          var bytes = new Uint8Array(memory.buffer);
          bytes.fill(0, buffer, buffer + 390);
          var parts = [[0, 'wasm'], [65, 'wasm'], [130, 'wasm'],
                       [195, '1.0'], [260, 'wasm'], [325, 'wasm']];
          for (var p = 0; p < parts.length; p++) {
            var offset = parts[p][0];
            var text = parts[p][1];
            for (var i = 0; i < text.length; i++) bytes[buffer + offset + i] = text.charCodeAt(i);
          }
          return 0;
        },
      },
    };

    return { imports: imports, attach: function (m) { memory = m; } };
  }

  function ascii(memory, pointer, length) {
    var bytes = new Uint8Array(memory.buffer, pointer, length);
    var text = '';
    for (var i = 0; i < bytes.length; i++) text += String.fromCharCode(bytes[i]);
    return text;
  }

  /** A 16-bit PCM WAV file from two Float32 channels (no Blob: worklets lack it). */
  function encodeWav(left, right, sampleRate) {
    var frames = left.length;
    var channels = right ? 2 : 1;
    var buffer = new ArrayBuffer(44 + frames * channels * 2);
    var view = new DataView(buffer);
    var text = function (offset, value) {
      for (var i = 0; i < value.length; i++) view.setUint8(offset + i, value.charCodeAt(i));
    };
    text(0, 'RIFF');
    view.setUint32(4, 36 + frames * channels * 2, true);
    text(8, 'WAVE');
    text(12, 'fmt ');
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);
    view.setUint16(22, channels, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, sampleRate * channels * 2, true);
    view.setUint16(32, channels * 2, true);
    view.setUint16(34, 16, true);
    text(36, 'data');
    view.setUint32(40, frames * channels * 2, true);
    var offset = 44;
    for (var i = 0; i < frames; i++) {
      for (var c = 0; c < channels; c++) {
        var sample = c === 0 ? left[i] : right[i];
        var clamped = Math.max(-1, Math.min(1, sample));
        view.setInt16(offset, clamped < 0 ? clamped * 0x8000 : clamped * 0x7fff, true);
        offset += 2;
      }
    }
    return buffer;
  }

  /**
   * Instantiate a unit and drive it.
   * @param wasm   the .wasm bytes
   * @param rate   sample rate
   * @param block  frames per unit_render call (the drumlogue uses 64)
   */
  async function create(wasm, rate, block) {
    var wasi = makeWasi();
    var compiled = await WebAssembly.instantiate(wasm, wasi.imports);
    var e = compiled.instance.exports;
    wasi.attach(e.memory);
    // Reactor builds export _initialize(): runs the units' C++ static
    // constructors. Without it some units initialise but never sound.
    if (typeof e._initialize === 'function') e._initialize();

    var result = e.host_init(rate, block);
    if (result !== 0) throw new Error('unit_init() returned ' + result);

    var length = e.host_manifest();
    var manifest = JSON.parse(ascii(e.memory, e.host_manifest_ptr(), length));

    var heap = new Float32Array(e.memory.buffer);
    var outPointer = e.host_out_ptr() >> 2;
    var inPointer = e.host_in_ptr() >> 2;
    var channels = e.host_channels();
    var inputPhase = 0;
    var inputMode = 'silence';

    return {
      manifest: manifest,
      sampleRate: rate,
      block: block,
      channels: channels,
      exports: e,

      noteOn: function (note, velocity) { e.host_note_on(note, velocity); },
      noteOff: function (note) { e.host_note_off(note); },
      allNotesOff: function () { e.host_all_note_off(); },
      setParam: function (id, value) { e.host_set_param(id, value); },
      getParam: function (id) { return e.host_get_param(id); },
      loadPreset: function (index) { e.host_load_preset(index); },
      presetIndex: function () { return e.host_get_preset_index(); },
      setTempo: function (bpm) { e.host_set_tempo(bpm); },
      pitchBend: function (value) { e.host_pitch_bend(value); },
      reset: function () { e.host_reset(); },
      paramValues: function () {
        var values = [];
        for (var id = 0; id < manifest.numParams && id < 24; id++) values.push(e.host_get_param(id));
        return values;
      },

      setInput: function (mode) { inputMode = mode; },

      /** Render `frames` into two channel arrays at `offset`. */
      renderInto: function (left, right, offset, frames) {
        for (var done = 0; done < frames; done += block) {
          var count = Math.min(block, frames - done);
          if (inputMode === 'tone') {
            for (var i = 0; i < count; i++) {
              var value = 0.25 * Math.sin((2 * Math.PI * 220 * (inputPhase + i)) / rate);
              heap[inPointer + i * 2] = value;
              heap[inPointer + i * 2 + 1] = value;
            }
            inputPhase += count;
          }
          e.host_render(count);
          for (var n = 0; n < count; n++) {
            var l = heap[outPointer + n * 2];
            var r = channels > 1 ? heap[outPointer + n * 2 + 1] : l;
            if (right) {
              left[offset + done + n] = l;
              right[offset + done + n] = r;
            } else {
              left[offset + done + n] = 0.5 * (l + r);
            }
          }
        }
      },

      /** Render `frames` as fast as possible; used by the benchmark. */
      benchmark: function (seconds) {
        var blocks = Math.round((seconds * rate) / block);
        var started = Date.now();
        for (var i = 0; i < blocks; i++) e.host_render(block);
        return Date.now() - started;
      },
    };
  }

  global.UnitHost = { create: create, encodeWav: encodeWav, makeWasi: makeWasi };
})(typeof globalThis !== 'undefined' ? globalThis : self);
