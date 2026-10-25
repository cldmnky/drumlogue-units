// AudioWorklet that runs a drumlogue unit through UnitHost.
//
// This file is concatenated after www/unit-host.js when it is loaded (the
// AudioWorklet global scope cannot import modules), so it must not have imports
// of its own. The processor renders the unit's own 64-frame blocks into the
// audio callback and can record what it plays.

class UnitProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.unit = null;
    this.block = 64;
    this.capture = null;
    this.heap = null;
    this.port.onmessage = (event) => this.handle(event.data);
  }

  async handle(message) {
    switch (message.type) {
      case 'load': {
        try {
          this.block = message.block || 64;
          this.unit = await UnitHost.create(message.wasm, sampleRate, this.block);
          this.port.postMessage({
            type: 'ready',
            manifest: this.unit.manifest,
            sampleRate: sampleRate,
            block: this.block,
            renderer: 'worklet',
          });
          this.port.postMessage({ type: 'paramValues', values: this.unit.paramValues() });
        } catch (error) {
          this.port.postMessage({ type: 'error', message: String(error) });
        }
        break;
      }
      case 'noteOn':
        this.unit?.noteOn(message.note, message.velocity ?? 100);
        break;
      case 'noteOff':
        this.unit?.noteOff(message.note);
        break;
      case 'allNotesOff':
        this.unit?.allNotesOff();
        break;
      case 'param':
        this.unit?.setParam(message.id, message.value);
        break;
      case 'preset': {
        this.unit?.loadPreset(message.index);
        if (this.unit) {
          this.port.postMessage({ type: 'preset', index: this.unit.presetIndex() });
          this.port.postMessage({ type: 'paramValues', values: this.unit.paramValues() });
        }
        break;
      }
      case 'tempo':
        this.unit?.setTempo(message.bpm);
        break;
      case 'pitchBend':
        this.unit?.pitchBend(message.value);
        break;
      case 'reset':
        this.unit?.reset();
        break;
      case 'input':
        this.unit?.setInput(message.mode);
        break;
      case 'benchmark': {
        if (!this.unit) break;
        const wall = this.unit.benchmark(message.seconds || 1);
        this.port.postMessage({ type: 'benchmark', wallMs: wall, seconds: message.seconds || 1 });
        break;
      }
      case 'startCapture':
        if (!this.unit) {
          this.port.postMessage({ type: 'error', message: 'start the unit first' });
          break;
        }
        this.capture = {
          left: [],
          right: [],
          frames: 0,
          limit: Math.round(message.seconds * sampleRate),
        };
        break;
      case 'stopCapture':
        this.finishCapture();
        break;
      default:
        break;
    }
  }

  finishCapture() {
    const capture = this.capture;
    if (!capture) return;
    this.capture = null;
    if (capture.frames === 0) {
      this.port.postMessage({ type: 'error', message: 'capture was empty' });
      return;
    }
    let peak = 0;
    let sum = 0;
    for (let i = 0; i < capture.frames; i++) {
      const value = Math.abs(capture.left[i]);
      if (value > peak) peak = value;
      sum += capture.left[i] * capture.left[i] + capture.right[i] * capture.right[i];
    }
    const wav = UnitHost.encodeWav(new Float32Array(capture.left),
                                   new Float32Array(capture.right), sampleRate);
    this.port.postMessage({
      type: 'capture',
      wav: wav,
      frames: capture.frames,
      peak: peak,
      rms: Math.sqrt(sum / (capture.frames * 2)),
    }, [wav]);
  }

  process(_inputs, outputs) {
    const out = outputs[0];
    if (!out || out.length === 0) return true;
    if (!this.unit) {
      for (const channel of out) channel.fill(0);
      return true;
    }
    const frames = out[0].length;
    const left = out[0];
    const right = out.length > 1 ? out[1] : null;
    this.unit.renderInto(left, right, 0, frames);

    if (this.capture) {
      for (let i = 0; i < frames && this.capture.frames < this.capture.limit; i++) {
        this.capture.left.push(left[i]);
        this.capture.right.push(right ? right[i] : left[i]);
        this.capture.frames++;
      }
      if (this.capture.frames >= this.capture.limit) this.finishCapture();
    }
    return true;
  }
}

registerProcessor('unit-processor', UnitProcessor);
