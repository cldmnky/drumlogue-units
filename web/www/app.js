// Web emulator UI: loads a drumlogue unit compiled to WebAssembly, runs it in an
// AudioWorklet and builds its controls from the unit's own header.
//
// No framework, no build step: the page loads this file, fetch()es the unit's
// .wasm and talks to the worklet over messages.

const state = {
  unit: null,
  manifest: null,
  context: null,
  node: null,
  send: null,            // (message) -> the active engine
  engineKind: 'none',    // 'worklet' or 'main'
  unit: null,            // main-thread unit (UnitHost instance)
  capture: null,         // main-thread capture state
  gain: null,
  ready: false,
  params: new Map(),        // index -> { param, input, label }
  notes: new Map(),         // note -> widget element
  presetIndex: 0,
  page: -1,                 // -1 shows every parameter
  paramsPerPage: 4,         // the drumlogue shows four at a time
  octave: 3,
  midi: null,
  readyPromise: null,
  resolveReady: null,
};

const UNITS_DIR = 'units/';
const UNIT_PARAM_NAMES = {
  none: '', percent: '%', db: ' dB', cents: ' ct', semi: ' st', oct: ' oct',
  hertz: ' Hz', khertz: ' kHz', bpm: ' BPM', msec: ' ms', sec: ' s',
  drywet: '', pan: '', spread: '', onoff: '', midi_note: '',
};

const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const noteName = (note) => `${NOTE_NAMES[note % 12]}${Math.floor(note / 12) - 1}`;

// --- unit list --------------------------------------------------------------

async function listUnits() {
  try {
    const response = await fetch(`${UNITS_DIR}index.json`, { cache: 'no-store' });
    if (response.ok) {
      const units = await response.json();
      if (Array.isArray(units) && units.length > 0) return units;
    }
  } catch (error) {
    // no index (opened from the filesystem, say)
  }
  return ['pepege-synth'];
}

// --- audio ------------------------------------------------------------------

async function startAudio() {
  if (state.context) {
    await unlockAudio();
    return;
  }

  // Creating and resuming the context must happen inside the click: browsers
  // only start audio from a gesture, and iOS additionally mutes Web Audio with
  // the hardware silent switch unless the session says it is playback.
  try {
    state.context = new AudioContext({ sampleRate: 48000, latencyHint: 'interactive' });
  } catch (error) {
    state.context = new AudioContext();
  }
  state.context.onstatechange = updateAudioState;
  iosAudioSession();
  await unlockAudio();
  updateAudioState();

  try {
    const wasm = await loadWasm();
    if (!wasm) return;
    const forced = new URLSearchParams(location.search).get('renderer');
    if (forced !== 'main' && state.context.audioWorklet && await startWorklet(wasm)) return;
    await startMainThread(wasm);
  } catch (error) {
    setStatus(`audio failed: ${error.message}`);
    console.error(error);
    showBanner('Audio could not start: ' + error.message);
  }
}

/**
 * iOS mutes Web Audio when the ringer switch is off unless the audio session is
 * "playback" (Safari) - Chrome on iOS has no AudioWorklet at all, hence the
 * main-thread renderer below.
 */
function iosAudioSession() {
  try {
    if (navigator.audioSession) navigator.audioSession.type = 'playback';
  } catch (error) {
    // not supported; the silent-switch workaround below may still help
  }
  // Playing a looping silent element moves the session to media playback on
  // iOS, which lifts the silent-switch mute for Web Audio too.
  let silent = document.getElementById('ios-unlock');
  if (!silent) {
    silent = document.createElement('audio');
    silent.id = 'ios-unlock';
    silent.loop = true;
    silent.setAttribute('playsinline', '');
    silent.src = 'data:audio/wav;base64,UklGRiQAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQAAAAA=';
    silent.style.display = 'none';
    document.body.appendChild(silent);
  }
  silent.play().catch(() => {});
}

async function loadWasm() {
  const response = await fetch(`${UNITS_DIR}${state.unit}.wasm`, { cache: 'no-store' });
  if (!response.ok) {
    setStatus(`cannot load ${state.unit}.wasm (${response.status})`);
    return null;
  }
  return response.arrayBuffer();
}

/** Render in an AudioWorklet (real-time thread). Returns false to fall back. */
async function startWorklet(wasm) {
  try {
    // The worklet cannot import modules, so its host code is concatenated in.
    const [host, processor] = await Promise.all([
      fetch(`unit-host.js?v=${Date.now()}`).then((r) => r.text()),
      fetch(`worklet.js?v=${Date.now()}`).then((r) => r.text()),
    ]);
    const url = URL.createObjectURL(new Blob([host, '\n', processor],
                                             { type: 'text/javascript' }));
    await state.context.audioWorklet.addModule(url);
    const node = new AudioWorkletNode(state.context, 'unit-processor', {
      numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2],
    });
    state.node = node;
    state.engineKind = 'worklet';
    state.send = (message) => node.port.postMessage(message);
    node.port.onmessage = onWorkletMessage;
    connectGain(node);

    state.readyPromise = new Promise((resolve) => { state.resolveReady = resolve; });
    state.send({ type: 'load', wasm, sampleRate: state.context.sampleRate, block: 64 });
    setStatus(`starting ${state.unit} in an AudioWorklet...`);
    setTimeout(() => {
      // No ready message means the worklet never ran (some WebKit builds).
      if (state.ready || state.engineKind !== 'worklet') return;
      setStatus('the AudioWorklet did not start; rendering on the main thread');
      state.engineKind = 'none';
      try { node.disconnect(); } catch (error) { /* already gone */ }
      state.node = null;
      state.send = null;
      startMainThread(wasm).catch((error) => setStatus(`fallback failed: ${error.message}`));
    }, 3000);
    return true;
  } catch (error) {
    console.warn('AudioWorklet unavailable, using the main thread:', error);
    return false;
  }
}

/** Render on the main thread with a ScriptProcessorNode (works everywhere). */
async function startMainThread(wasm) {
  const block = 64;
  const unit = await UnitHost.create(wasm, state.context.sampleRate, block);
  state.unit = unit;
  state.engineKind = 'main';

  const bufferSize = 1024;
  const node = state.context.createScriptProcessor(bufferSize, 0, 2);
  node.onaudioprocess = (event) => {
    const left = event.outputBuffer.getChannelData(0);
    const right = event.outputBuffer.getChannelData(1);
    unit.renderInto(left, right, 0, left.length);
    if (state.capture) captureMainThread(left, right);
  };
  state.node = node;
  connectGain(node);

  state.send = (message) => sendMainThread(unit, message);
  state.ready = true;
  state.resolveReady?.();
  onWorkletMessage({
    data: {
      type: 'ready',
      manifest: unit.manifest,
      sampleRate: state.context.sampleRate,
      block: bufferSize,
      renderer: 'main thread',
    },
  });
  state.send({ type: 'input', mode: document.getElementById('input').value });
  setStatus(`${unit.manifest.name} on the main thread | audio ${state.context.state}`);
}

function connectGain(node) {
  state.gain = state.context.createGain();
  state.gain.gain.value = Number(document.getElementById('volume').value);
  node.connect(state.gain).connect(state.context.destination);
}

/** The UI's messages, handled locally when the page renders on the main thread. */
function sendMainThread(unit, message) {
  switch (message.type) {
    case 'noteOn': unit.noteOn(message.note, message.velocity ?? 100); break;
    case 'noteOff': unit.noteOff(message.note); break;
    case 'allNotesOff': unit.allNotesOff(); break;
    case 'param': unit.setParam(message.id, message.value); break;
    case 'preset': unit.loadPreset(message.index);
      onWorkletMessage({ data: { type: 'preset', index: unit.presetIndex() } });
      onWorkletMessage({ data: { type: 'paramValues', values: unit.paramValues() } });
      break;
    case 'tempo': unit.setTempo(message.bpm); break;
    case 'pitchBend': unit.pitchBend(message.value); break;
    case 'reset': unit.reset(); break;
    case 'input': unit.setInput(message.mode); break;
    case 'startCapture':
      state.capture = { left: [], right: [], frames: 0,
                        limit: Math.round(message.seconds * state.context.sampleRate) };
      break;
    case 'benchmark': {
      const wall = unit.benchmark(message.seconds || 1);
      onWorkletMessage({ data: { type: 'benchmark', wallMs: wall,
                                 seconds: message.seconds || 1 } });
      break;
    }
    default: break;
  }
}

function captureMainThread(left, right) {
  const capture = state.capture;
  for (let i = 0; i < left.length && capture.frames < capture.limit; i++) {
    capture.left.push(left[i]);
    capture.right.push(right[i]);
    capture.frames++;
  }
  if (capture.frames < capture.limit) return;
  state.capture = null;
  let peak = 0;
  let sum = 0;
  for (let i = 0; i < capture.frames; i++) {
    const value = Math.abs(capture.left[i]);
    if (value > peak) peak = value;
    sum += capture.left[i] * capture.left[i] + capture.right[i] * capture.right[i];
  }
  const wav = UnitHost.encodeWav(new Float32Array(capture.left),
                                 new Float32Array(capture.right),
                                 state.context.sampleRate);
  onWorkletMessage({ data: { type: 'capture', wav, frames: capture.frames, peak,
                             rms: Math.sqrt(sum / (capture.frames * 2)) } });
}

function showBanner(text) {
  const banner = document.getElementById('banner');
  if (!banner) return;
  banner.textContent = text;
  banner.hidden = false;
}

/** Resume the audio context; browsers start it suspended until a gesture. */
async function unlockAudio() {
  if (!state.context) return;
  try {
    if (state.context.state !== 'running') await state.context.resume();
  } catch (error) {
    console.error('resume failed:', error);
  }
  updateAudioState();
}

function updateAudioState() {
  const state_ = state.context?.state ?? 'none';
  const label = document.getElementById('audio-state');
  if (label) label.textContent = state_;
  if (state.context) {
    document.body.classList.toggle('suspended', state_ !== 'running');
  }
}

// A beep that bypasses the unit: proves the browser's audio path works.
function beep() {
  if (!state.context) {
    startAudio().then(beep);
    return;
  }
  const oscillator = state.context.createOscillator();
  const gain = state.context.createGain();
  oscillator.frequency.value = 440;
  gain.gain.value = 0.1;
  oscillator.connect(gain).connect(state.context.destination);
  oscillator.start();
  oscillator.stop(state.context.currentTime + 0.4);
  setStatus('beep 440 Hz - if you cannot hear it, the problem is the browser or the device volume');
}

function onWorkletMessage(event) {
  const message = event.data;
  switch (message.type) {
    case 'ready':
      state.manifest = message.manifest;
      state.ready = true;
      state.resolveReady?.();
      if (message.manifest.module === 'effect') {
        document.getElementById('input').value = 'tone';
        state.send?.({ type: 'input', mode: 'tone' });
      }
      document.getElementById('unit-name').textContent = message.manifest.name;
      document.body.classList.remove('loading');
      buildControls(message.manifest);
      setStatus(`${message.manifest.name} | ${message.manifest.module} | ` +
                `${message.sampleRate} Hz | block ${message.block} | ` +
                `${message.renderer ?? 'worklet'} | audio ${state.context?.state ?? '?'}`);
      break;
    case 'error':
      state.resolveReady?.();
      setStatus(`error: ${message.message}`);
      console.error('unit error:', message.message);
      break;
    case 'benchmark': {
      const realtime = (message.seconds * 1000) / Math.max(1, message.wallMs);
      document.getElementById('load').textContent =
        `${(100 / realtime).toFixed(1)}% (${realtime.toFixed(0)}x real time)`;
      break;
    }
    case 'paramValues':
      message.values.forEach((value, index) => setControlValue(index, value));
      break;
    case 'preset':
      state.presetIndex = message.index;
      document.getElementById('presets').value = String(message.index);
      break;
    case 'capture':
      setStatus(`recorded ${(message.frames / (state.context?.sampleRate || 48000)).toFixed(2)} s` +
                ` | peak ${message.peak.toFixed(3)} | rms ${message.rms.toFixed(4)}`);
      document.getElementById('level').textContent =
        `peak ${message.peak.toFixed(3)} rms ${message.rms.toFixed(4)}`;
      if (state.downloadCapture) downloadWav(message.wav);
      state.downloadCapture = false;
      break;
    default:
      break;
  }
}

function setStatus(text) {
  document.getElementById('status').textContent = text;
}

// --- parameter controls -----------------------------------------------------

function valueName(param, value) {
  const distinct = param.values ? new Set(param.values).size : 0;
  if (param.values && distinct > 1 &&
      (param.typeName === 'strings' || param.typeName === 'enum')) {
    const index = value - (param.valueMin ?? param.min);
    return param.values[index] ?? String(value);
  }
  return `${value}${UNIT_PARAM_NAMES[param.typeName] ?? ''}`;
}

function buildControls(manifest) {
  const container = document.getElementById('params');
  container.innerHTML = '';
  state.params.clear();
  buildPages(manifest);

  for (const param of manifest.params) {
    const row = document.createElement('div');
    row.className = 'param';

    const name = document.createElement('label');
    name.textContent = param.name;
    name.htmlFor = `param-${param.index}`;
    row.appendChild(name);

    const value = document.createElement('span');
    value.className = 'value';
    row.appendChild(value);

    const distinct = param.values ? new Set(param.values).size : 0;
    const useSelect = param.values && distinct > 1 &&
                      (param.typeName === 'strings' || param.typeName === 'enum');
    let input;
    if (useSelect) {
      input = document.createElement('select');
      param.values.forEach((text, i) => {
        const option = document.createElement('option');
        option.value = String((param.valueMin ?? param.min) + i);
        option.textContent = text;
        input.appendChild(option);
      });
    } else {
      input = document.createElement('input');
      input.type = 'range';
      input.min = String(param.min);
      input.max = String(param.max);
      input.step = '1';
    }
    input.id = `param-${param.index}`;
    input.addEventListener('input', () => {
      const numeric = Number(input.value);
      state.send?.({ type: 'param', id: param.index, value: numeric });
      value.textContent = valueName(param, numeric);
    });

    row.appendChild(input);
    container.appendChild(row);
    state.params.set(param.index, { param, input, label: value });
    setControlValue(param.index, param.init ?? param.min);
  }

  const presets = document.getElementById('presets');
  presets.innerHTML = '';
  for (const preset of manifest.presets) {
    const option = document.createElement('option');
    option.value = String(preset.index);
    option.textContent = preset.name;
    presets.appendChild(option);
  }
  presets.disabled = manifest.presets.length === 0;
  if (manifest.presets.length > 0) {
    presets.value = String(state.presetIndex);
    presets.dispatchEvent(new Event('change'));
  }
}

// The drumlogue groups its parameters into pages of four; offer the same view.
function buildPages(manifest) {
  const pages = document.getElementById('pages');
  pages.innerHTML = '';
  const count = Math.ceil(manifest.params.length / state.paramsPerPage);
  const options = [{ page: -1, label: 'All' }];
  for (let page = 0; page < count; page++) options.push({ page, label: String(page + 1) });
  for (const option of options) {
    const button = document.createElement('button');
    button.textContent = option.label;
    button.classList.toggle('active', option.page === state.page);
    button.addEventListener('click', () => {
      state.page = option.page;
      document.querySelectorAll('#pages button').forEach((each) => each.classList.remove('active'));
      button.classList.add('active');
      applyPageFilter();
    });
    pages.appendChild(button);
  }
  applyPageFilter();
}

function applyPageFilter() {
  for (const { param, input } of state.params.values()) {
    const visible = state.page < 0 ||
      Math.floor(param.index / state.paramsPerPage) === state.page;
    const row = input.closest('.param');
    if (row) row.style.display = visible ? '' : 'none';
  }
}

function setControlValue(index, value) {
  const entry = state.params.get(index);
  if (!entry || document.activeElement === entry.input) return;
  entry.input.value = String(value);
  entry.label.textContent = valueName(entry.param, value);
}

// --- keyboard ---------------------------------------------------------------

function buildKeyboard() {
  const keyboard = document.getElementById('keyboard');
  const white = [0, 2, 4, 5, 7, 9, 11];
  const black = { 1: 0, 3: 1, 6: 3, 8: 4, 10: 5 };
  for (let octave = 0; octave < 2; octave++) {
    const octaveElement = document.createElement('div');
    octaveElement.className = 'octave';
    for (const semitone of white) {
      const key = document.createElement('button');
      key.className = 'key white';
      key.dataset.note = String((state.octave + octave) * 12 + semitone);
      key.textContent = noteName(Number(key.dataset.note));
      octaveElement.appendChild(key);
      state.notes.set(key.dataset.note, key);
    }
    const octaveWidth = 100 / 7;      // percent of an octave per white key
    const blackWidth = 9;             // percent, matches .key.black in the CSS
    for (const [semitone, slot] of Object.entries(black)) {
      const key = document.createElement('button');
      key.className = 'key black';
      // Centre the black key on the boundary after `slot` white keys.
      key.style.left = `${(Number(slot) + 1) * octaveWidth - blackWidth / 2}%`;
      key.dataset.note = String((state.octave + octave) * 12 + Number(semitone));
      key.textContent = '';
      octaveElement.appendChild(key);
      state.notes.set(key.dataset.note, key);
    }
    keyboard.appendChild(octaveElement);
  }

  keyboard.addEventListener('pointerdown', (event) => {
    const key = event.target.closest('.key');
    if (!key) return;
    event.preventDefault();
    key.setPointerCapture(event.pointerId);
    noteOn(Number(key.dataset.note));
  });
  keyboard.addEventListener('pointerup', (event) => {
    const key = event.target.closest('.key');
    if (!key) return;
    noteOff(Number(key.dataset.note));
  });
  keyboard.addEventListener('pointercancel', (event) => {
    const key = event.target.closest('.key');
    if (key) noteOff(Number(key.dataset.note));
  });
}

function noteOn(note) {
  const key = state.notes.get(String(note));
  key?.classList.add('down');
  const send = () => state.send?.({ type: 'noteOn', note, velocity: 100 });

  // Playing a key before "Start audio" is the most common way to get silence:
  // the key press is itself a user gesture, so start from it.
  if (!state.context) {
    startAudio()
      .then(async () => {
        await state.readyPromise;
        if (key?.classList.contains('down')) send();
      })
      .catch(() => {});
    return;
  }
  if (!state.ready) {
    setStatus('loading the unit...');
    return;
  }
  send();
}

function noteOff(note) {
  const key = state.notes.get(String(note));
  key?.classList.remove('down');
  state.send?.({ type: 'noteOff', note });
}

const COMPUTER_KEYS = {
  z: 0, s: 1, x: 2, d: 3, c: 4, v: 5, g: 6, b: 7, h: 8, n: 9, j: 10, m: 11,
  q: 12, '2': 13, w: 14, '3': 15, e: 16, r: 17, '5': 18, t: 19, '6': 20, y: 21,
  '7': 22, u: 23, i: 24,
};

window.addEventListener('keydown', (event) => {
  if (event.repeat || event.target.tagName === 'INPUT' || event.target.tagName === 'SELECT') return;
  const offset = COMPUTER_KEYS[event.key.toLowerCase()];
  if (offset === undefined) return;
  event.preventDefault();
  noteOn(state.octave * 12 + offset);
});

window.addEventListener('blur', allNotesOff);

window.addEventListener('keyup', (event) => {
  const offset = COMPUTER_KEYS[event.key.toLowerCase()];
  if (offset === undefined) return;
  noteOff(state.octave * 12 + offset);
});

// --- MIDI -------------------------------------------------------------------

async function enableMidi() {
  if (!navigator.requestMIDIAccess) {
    setStatus('no Web MIDI in this browser');
    return;
  }
  try {
    state.midi = await navigator.requestMIDIAccess();
    const attach = (input) => { input.onmidimessage = onMidi; };
    state.midi.inputs.forEach(attach);
    state.midi.onstatechange = (event) => attach(event.port);
    setStatus(`MIDI: ${[...state.midi.inputs.values()].map((i) => i.name).join(', ') || 'no inputs'}`);
  } catch (error) {
    setStatus(`MIDI refused: ${error.message}`);
  }
}

function onMidi(event) {
  const [status, data1, data2] = event.data;
  const command = status & 0xf0;
  if (command === 0x90 && data2 > 0) noteOn(data1);
  else if (command === 0x80 || (command === 0x90 && data2 === 0)) noteOff(data1);
  else if (command === 0xe0) {
    state.send?.({ type: 'pitchBend', value: (data2 << 7) | data1 });
  }
}

// --- recording --------------------------------------------------------------

function downloadWav(buffer) {
  const blob = new Blob([buffer], { type: 'audio/wav' });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = `${state.unit}-${Date.now()}.wav`;
  link.click();
  setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

// --- wiring -----------------------------------------------------------------

document.addEventListener('pointerdown', () => {
  if (state.context && state.context.state !== 'running') unlockAudio();
});

async function boot() {
  const params = new URLSearchParams(location.search);
  const units = await listUnits();
  state.unit = params.get('unit') || units[0];

  const picker = document.getElementById('units');
  for (const unit of units) {
    const option = document.createElement('option');
    option.value = unit;
    option.textContent = unit;
    picker.appendChild(option);
  }
  picker.value = state.unit;
  picker.addEventListener('change', () => {
    location.search = `?unit=${encodeURIComponent(picker.value)}`;
  });

  document.getElementById('octave').textContent = noteName(state.octave * 12);
  buildKeyboard();

  document.getElementById('start').addEventListener('click', startAudio);
  document.getElementById('beep').addEventListener('click', beep);
  document.getElementById('panic').addEventListener('click', allNotesOff);
  document.getElementById('presets').addEventListener('change', (event) => {
    state.send?.({ type: 'preset', index: Number(event.target.value) });
  });
  document.getElementById('volume').addEventListener('input', (event) => {
    if (state.gain) state.gain.gain.value = Number(event.target.value);
  });
  document.getElementById('input').addEventListener('change', (event) => {
    state.send?.({ type: 'input', mode: event.target.value });
  });
  document.getElementById('midi').addEventListener('click', enableMidi);
  document.getElementById('benchmark').addEventListener('click', async () => {
    if (!state.context) await startAudio();
    await state.readyPromise;
    state.send?.({ type: 'benchmark', seconds: 1 });
  });
  document.getElementById('selftest').addEventListener('click', async () => {
    if (!state.context) await startAudio();
    await state.readyPromise;
    if (!state.ready) { setStatus('unit is not ready yet'); return; }
    state.send?.({ type: 'preset', index: 0 });
    state.send?.({ type: 'noteOn', note: 60, velocity: 100 });
    state.send?.({ type: 'noteOn', note: 64, velocity: 100 });
    state.downloadCapture = true;
    state.send?.({ type: 'startCapture', seconds: 2 });
    setTimeout(() => {
      state.send?.({ type: 'noteOff', note: 60 });
      state.send?.({ type: 'noteOff', note: 64 });
    }, 1500);
  });
  document.getElementById('record').addEventListener('click', () => {
    if (!state.node) return;
    state.downloadCapture = true;
    state.send?.({ type: 'startCapture', seconds: 2 });
    setStatus('recording 2 s...');
  });
  document.getElementById('octave-down').addEventListener('click', () => shiftOctave(-1));
  document.getElementById('octave-up').addEventListener('click', () => shiftOctave(1));

  setStatus(`${state.unit}: press "Start audio"`);
}

function allNotesOff() {
  state.send?.({ type: 'allNotesOff' });
  document.querySelectorAll('.key.down').forEach((key) => key.classList.remove('down'));
}

function shiftOctave(delta) {
  const next = Math.min(7, Math.max(0, state.octave + delta));
  if (next === state.octave) return;
  allNotesOff();
  state.octave = next;
  document.getElementById('keyboard').innerHTML = '';
  state.notes.clear();
  buildKeyboard();
  document.getElementById('octave').textContent = noteName(state.octave * 12);
}

boot();
