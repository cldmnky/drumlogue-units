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
  gain: null,
  ready: false,
  params: new Map(),        // index -> { param, input, label }
  notes: new Map(),         // note -> widget element
  presetIndex: 0,
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
  if (state.context) return;
  const requested = 48000;
  state.context = new AudioContext({ sampleRate: requested, latencyHint: 'interactive' });
  // Cache-busted: browsers hold on to worklet modules across reloads.
  await state.context.audioWorklet.addModule(`worklet.js?v=${Date.now()}`);
  state.node = new AudioWorkletNode(state.context, 'unit-processor', {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });
  state.gain = state.context.createGain();
  state.gain.gain.value = Number(document.getElementById('volume').value);
  state.node.connect(state.gain).connect(state.context.destination);
  state.node.port.onmessage = onWorkletMessage;

  const response = await fetch(`${UNITS_DIR}${state.unit}.wasm`, { cache: 'no-store' });
  if (!response.ok) {
    setStatus(`cannot load ${state.unit}.wasm (${response.status}) - run "make" in web/`);
    return;
  }
  const wasm = await response.arrayBuffer();
  state.readyPromise = new Promise((resolve) => { state.resolveReady = resolve; });
  state.node.port.postMessage({ type: 'load', wasm, sampleRate: state.context.sampleRate,
                                block: 64 });
  setStatus(`starting ${state.unit}...`);
  await state.context.resume();
}

function onWorkletMessage(event) {
  const message = event.data;
  switch (message.type) {
    case 'ready':
      state.manifest = message.manifest;
      state.ready = true;
      state.resolveReady?.();
      document.getElementById('unit-name').textContent = message.manifest.name;
      document.body.classList.remove('loading');
      buildControls(message.manifest);
      setStatus(`${message.manifest.name} | ${message.sampleRate} Hz | block ${message.block}`);
      break;
    case 'error':
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
      state.node?.port.postMessage({ type: 'param', id: param.index, value: numeric });
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
  state.node?.port.postMessage({ type: 'noteOn', note, velocity: 100 });
}

function noteOff(note) {
  const key = state.notes.get(String(note));
  key?.classList.remove('down');
  state.node?.port.postMessage({ type: 'noteOff', note });
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
    state.node?.port.postMessage({ type: 'pitchBend', value: (data2 << 7) | data1 });
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
  document.getElementById('panic').addEventListener('click', () => {
    state.node?.port.postMessage({ type: 'allNotesOff' });
    document.querySelectorAll('.key.down').forEach((key) => key.classList.remove('down'));
  });
  document.getElementById('presets').addEventListener('change', (event) => {
    state.node?.port.postMessage({ type: 'preset', index: Number(event.target.value) });
  });
  document.getElementById('volume').addEventListener('input', (event) => {
    if (state.gain) state.gain.gain.value = Number(event.target.value);
  });
  document.getElementById('midi').addEventListener('click', enableMidi);
  document.getElementById('benchmark').addEventListener('click', async () => {
    if (!state.context) await startAudio();
    await state.readyPromise;
    state.node.port.postMessage({ type: 'benchmark', seconds: 1 });
  });
  document.getElementById('selftest').addEventListener('click', async () => {
    if (!state.context) await startAudio();
    await state.readyPromise;
    if (!state.ready) { setStatus('unit is not ready yet'); return; }
    state.node.port.postMessage({ type: 'preset', index: 0 });
    state.node.port.postMessage({ type: 'noteOn', note: 60, velocity: 100 });
    state.node.port.postMessage({ type: 'noteOn', note: 64, velocity: 100 });
    state.downloadCapture = true;
    state.node.port.postMessage({ type: 'startCapture', seconds: 2 });
    setTimeout(() => {
      state.node.port.postMessage({ type: 'noteOff', note: 60 });
      state.node.port.postMessage({ type: 'noteOff', note: 64 });
    }, 1500);
  });
  document.getElementById('record').addEventListener('click', () => {
    if (!state.node) return;
    state.downloadCapture = true;
    state.node.port.postMessage({ type: 'startCapture', seconds: 2 });
    setStatus('recording 2 s...');
  });
  document.getElementById('octave-down').addEventListener('click', () => shiftOctave(-1));
  document.getElementById('octave-up').addEventListener('click', () => shiftOctave(1));

  setStatus(`${state.unit}: press "Start audio"`);
}

function shiftOctave(delta) {
  const next = Math.min(7, Math.max(0, state.octave + delta));
  if (next === state.octave) return;
  state.octave = next;
  document.getElementById('keyboard').innerHTML = '';
  state.notes.clear();
  buildKeyboard();
  document.getElementById('octave').textContent = noteName(state.octave * 12);
}

boot();
