// AudioWorklet that runs a drumlogue unit compiled to WebAssembly.
//
// The module (web/www/units/<unit>.wasm) is the unit's own DSP plus
// src/unit_shim.c, which exposes host_* functions. This file instantiates it
// with the handful of WASI imports Emscripten's standalone build expects and
// calls host_render() for every block of the audio callback, in the drumlogue's
// own buffer size (64 frames).

const DEFAULT_BLOCK = 64;

// --- the few WASI imports a standalone Emscripten module asks for -----------
function makeWasi() {
  let memory = null;
  const view = () => new DataView(memory.buffer);
  const writeU32 = (ptr, value) => view().setUint32(ptr, value, true);
  const writeU64 = (ptr, value) => view().setBigUint64(ptr, BigInt(value), true);

  const imports = {
    wasi_snapshot_preview1: {
      proc_exit: () => { throw new Error('unit called proc_exit'); },
      fd_write: () => 0,
      fd_close: () => 0,
      fd_seek: () => 0,
      fd_fdstat_get: () => 0,
      fd_fdstat_set_flags: () => 0,
      fd_prestat_get: () => 8,          // EBADF: no preopens
      fd_prestat_dir_name: () => 8,
      environ_get: () => 0,
      environ_sizes_get: (countPtr, sizePtr) => {
        writeU32(countPtr, 0);
        writeU32(sizePtr, 0);
        return 0;
      },
      clock_time_get: (id, precision, outPtr) => {
        writeU64(outPtr, BigInt(Date.now()) * 1000000n);
        return 0;
      },
      random_get: (ptr, length) => {
        const bytes = new Uint8Array(memory.buffer, ptr, length);
        for (let i = 0; i < length; i++) bytes[i] = (Math.random() * 256) | 0;
        return 0;
      },
      sched_yield: () => 0,
      poll_oneoff: () => 0,
      clock_res_get: (_id, outPtr) => {
        writeU64(outPtr, 1000n);
        return 0;
      },
    },
    env: {
      emscripten_notify_memory_growth: () => {},
      emscripten_memcpy_big: (dest, src, num) => {
        new Uint8Array(memory.buffer).copyWithin(dest, src, src + num);
        return dest;
      },
    },
  };

  return { imports, attach: (m) => { memory = m; } };
}

// --- a WAV file from Float32 blocks -----------------------------------------
function encodeWav(channels, sampleRate) {
  const frames = channels[0].length;
  const channelCount = channels.length;
  const buffer = new ArrayBuffer(44 + frames * channelCount * 2);
  const view = new DataView(buffer);
  const text = (offset, value) => {
    for (let i = 0; i < value.length; i++) view.setUint8(offset + i, value.charCodeAt(i));
  };
  text(0, 'RIFF');
  view.setUint32(4, 36 + frames * channelCount * 2, true);
  text(8, 'WAVE');
  text(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);                 // PCM
  view.setUint16(22, channelCount, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * channelCount * 2, true);
  view.setUint16(32, channelCount * 2, true);
  view.setUint16(34, 16, true);
  text(36, 'data');
  view.setUint32(40, frames * channelCount * 2, true);
  let offset = 44;
  for (let i = 0; i < frames; i++) {
    for (let c = 0; c < channelCount; c++) {
      const clamped = Math.max(-1, Math.min(1, channels[c][i]));
      view.setInt16(offset, clamped < 0 ? clamped * 0x8000 : clamped * 0x7fff, true);
      offset += 2;
    }
  }
  return buffer;
}

class UnitProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.exports = null;
    this.memory = null;
    this.block = DEFAULT_BLOCK;
    this.channels = 2;
    this.ready = false;
    this.noteCount = 0;
    this.capture = null;          // {frames, left, right} while recording
    this.reportAt = 0;
    this.windowStarted = 0;
    this.blocks = 0;
    this.port.onmessage = (event) => this.handle(event.data);
  }

  async handle(message) {
    switch (message.type) {
      case 'load':
        await this.load(message);
        break;
      case 'noteOn':
        if (this.ready) this.exports.host_note_on(message.note, message.velocity ?? 100);
        break;
      case 'noteOff':
        if (this.ready) this.exports.host_note_off(message.note);
        break;
      case 'allNotesOff':
        if (this.ready) this.exports.host_all_note_off();
        break;
      case 'param':
        if (this.ready) this.exports.host_set_param(message.id, message.value);
        break;
      case 'preset':
        if (this.ready) {
          this.exports.host_load_preset(message.index);
          this.port.postMessage({ type: 'preset', index: this.exports.host_get_preset_index() });
          this.sendParamValues();
        }
        break;
      case 'tempo':
        if (this.ready) this.exports.host_set_tempo(message.bpm);
        break;
      case 'pitchBend':
        if (this.ready) this.exports.host_pitch_bend(message.value);
        break;
      case 'reset':
        if (this.ready) this.exports.host_reset();
        break;
      case 'params':
        this.sendParamValues();
        break;
      case 'benchmark': {
        if (!this.ready) break;
        const seconds = message.seconds || 1;
        const blocks = Math.round((seconds * sampleRate) / this.block);
        const started = Date.now();
        for (let i = 0; i < blocks; i++) this.exports.host_render(this.block);
        const wall = Date.now() - started;
        this.port.postMessage({ type: 'benchmark', wallMs: wall, seconds, blocks });
        break;
      }
      case 'startCapture':
        this.capture = { left: [], right: [], frames: 0,
                         limit: Math.round(message.seconds * sampleRate) };
        break;
      case 'stopCapture':
        this.finishCapture();
        break;
      default:
        break;
    }
  }

  async load({ wasm, sampleRate: rate, block }) {
    if (this.ready) {
      this.exports.host_teardown();
      this.ready = false;
    }
    this.block = block || DEFAULT_BLOCK;
    try {
      const wasi = makeWasi();
      const { instance } = await WebAssembly.instantiate(wasm, wasi.imports);
      this.exports = instance.exports;
      this.memory = instance.exports.memory;
      wasi.attach(this.memory);
      this.channels = this.exports.host_channels();

      const result = this.exports.host_init(rate, this.block);
      if (result !== 0) {
        this.port.postMessage({ type: 'error', message: `unit_init() returned ${result}` });
        return;
      }
      this.ready = true;

      const length = this.exports.host_manifest();
      const pointer = this.exports.host_manifest_ptr();
      const bytes = new Uint8Array(this.memory.buffer, pointer, length);
      let json = '';
      for (let i = 0; i < bytes.length; i++) json += String.fromCharCode(bytes[i]);
      this.port.postMessage({ type: 'ready', manifest: JSON.parse(json), sampleRate: rate,
                              block: this.block });
      this.sendParamValues();
    } catch (error) {
      this.port.postMessage({ type: 'error', message: String(error) });
    }
  }

  sendParamValues() {
    if (!this.ready) return;
    const values = [];
    for (let id = 0; id < 24; id++) values.push(this.exports.host_get_param(id));
    this.port.postMessage({ type: 'paramValues', values });
  }

  finishCapture() {
    if (!this.capture) return;
    const { left, right, frames } = this.capture;
    this.capture = null;
    if (frames === 0) {
      this.port.postMessage({ type: 'error', message: 'capture was empty' });
      return;
    }
    let peak = 0;
    let sum = 0;
    for (let i = 0; i < frames; i++) {
      const value = Math.abs(left[i]);
      if (value > peak) peak = value;
      sum += left[i] * left[i] + right[i] * right[i];
    }
    const wav = encodeWav([new Float32Array(left), new Float32Array(right)], sampleRate);
    this.port.postMessage({ type: 'capture', wav, frames, peak,
                            rms: Math.sqrt(sum / (frames * 2)) }, [wav]);
  }

  process(_inputs, outputs) {
    const out = outputs[0];
    if (!out || out.length === 0) return true;
    if (!this.ready) {
      for (const channel of out) channel.fill(0);
      return true;
    }

    const frames = out[0].length;
    const heap = new Float32Array(this.memory.buffer);
    const outPointer = this.exports.host_out_ptr() >> 2;
    if (this.windowStarted === 0) this.windowStarted = Date.now();

    for (let offset = 0; offset < frames; offset += this.block) {
      const count = Math.min(this.block, frames - offset);
      this.exports.host_render(count);
      for (let i = 0; i < count; i++) {
        const left = heap[outPointer + i * 2];
        const right = this.channels > 1 ? heap[outPointer + i * 2 + 1] : left;
        if (out.length > 1) {
          out[0][offset + i] = left;
          out[1][offset + i] = right;
        } else {
          out[0][offset + i] = 0.5 * (left + right);
        }
      }
      if (this.capture) {
        for (let i = 0; i < count && this.capture.frames < this.capture.limit; i++) {
          this.capture.left.push(heap[outPointer + i * 2]);
          this.capture.right.push(this.channels > 1 ? heap[outPointer + i * 2 + 1] : heap[outPointer + i * 2]);
          this.capture.frames++;
        }
        if (this.capture.frames >= this.capture.limit) this.finishCapture();
      }
    }

    this.blocks++;
    return true;
  }
}

registerProcessor('unit-processor', UnitProcessor);
