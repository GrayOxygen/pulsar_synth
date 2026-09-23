# Pulsar Synth (Pulsar Wang)

An experimental **Pulsar / Pulsaret** synthesizer plugin built with **JUCE**, implementing the **Pulsar Synthesis** concept from Curtis Roads' *Microsound*. Project name `pulsar_synth`, product name **"Pulsar Wang"** (v0.0.8), CMake + C++20, dual platform: macOS (AU / VST3 / Standalone) and Windows (VST3).

Core idea: a **Train (pulse train)** state machine organizes a number of **pulsar periods**; each period triggers a **grain spawn**. At the bottom, a **GranularEngine** renders all active pulsarets (wavetable frame × window) via overlap-add, and almost every parameter is modulated by **hand-drawable envelope canvases**, each advancing along its own timeline.

> 中文版见 [README.md](README.md)

---

## Sound Engine

### Train State Machine

- **BPM**: the plugin's internal tempo (independent of the DAW), advanced by the **BPM envelope** in latch mode — one envelope point per completed stage.
- **Train Len**: how many "beat blocks" one train spans (`trainLenBlock = 60/bpm/beatDivision`).
- **Train Duty Cycle**: how many pulsar periods the sounding section of a train contains.
- **Train Silence**: the silent gap between trains (counted in pulsar periods).

```
pulsarPeriod = trainTime / (trainDutyCycle + trainSilence)
fundamentalFreq = 1 / pulsarPeriod
```

The state machine switches between `Pulse / IntraSilence / InterTrainSilence`; parameter changes take effect at silence boundaries to avoid hard cuts. All train-related quantities are stored in a **snapshot** at trigger time so computation stays consistent within a period.

### Granular Rendering (GranularEngine)

Each trigger (after passing the mask gate) spawns a group of grains, which the engine overlap-adds sample by sample:

- **Grain pool**: up to 32 concurrent grains; the oldest is dropped when the pool is full.
- **Pulsaret waveform**: comes from a **multi-frame wavetable** (up to 32 frames × 9600 samples). Draw a single frame on the Pg Waveform canvas, or load up to 32 wav files at once as a frame sequence; **wavetable scanning** ping-pongs across the frames with linear morphing between neighbors, and **Wave Scan Speed** controls the scan rate. Each frame's hann-weighted DC component is subtracted at lookup time, eliminating DC offset at the source.
- **Per-grain window**: a full-cycle raised-cosine window × an adjustable **grain ADSR** window (A/D/R as fractions of the grain lifetime, S as level).
- **Per-grain pan**: equal-power panning (√2 compensation, center = unity gain). The pan value is latched from the **Pan envelope** at spawn time — pulsarets sweep across the stereo field as the envelope moves, while the tails of already-sounding grains stay in place.
- **Per-pulsaret LFOs**: the AM / FM LFO canvases use the grain's windowPhase as their phase, modulating amplitude / pitch inside each individual pulsaret.
- **Duty Cycle Cluster**: one trigger's grain sweeps through multiple waveform cycles (controlled by the cluster envelope), creating pulse-cluster / formant textures.

### Pulsar Extension

- **Harmonics**: each trigger additionally spawns coherent harmonic layers at 3×/5× the fundamental, levels decaying by 1/n, with independent slow Perlin curves producing "partial breathing".
- **Unison Detune**: additionally spawns a pair of ±cents detuned grain copies; cent-level detuning yields slow beating (supersaw-style thickness).
- **Cellular automaton / Perlin noise**: the active cell count and Perlin modulation contribute micro-variations to scan frequency and duration.

### Rhythmic Masks (apply to the Train Duty Cycle section only)

| Type | Description |
|------|-------------|
| **Off** | Every pulsar period triggers |
| **Burst Mask** | User 0/1 string (e.g. `1011`), gated by period index |
| **Euclid Mask** | Euclidean rhythm generated from `steps` / `hits` |
| **Stochastic Mask** | Random 0/1 sequence generated from the train duty cycle length |

Mask = trigger gating: masked periods spawn no new grains but never cut off the tails of grains already sounding.

---

## Modulation System: Drawable Envelopes + De-locked Timelines

All envelopes are drawn on an **EnvelopeCanvas** (9600 points, linear-interpolated lookup), with Clear / Random / Load File:

| Envelope | Page | Role |
|----------|------|------|
| **AM Env** | Train Envelope | Latches each grain's amplitude at trigger time |
| **FM Env** | Train Envelope | Latches the scan-frequency offset at trigger time |
| **Pan Env** | Train Envelope | Latches each grain's pan at trigger time (0=left, 0.5=center, 1=right; Amount scales the deviation from center) |
| **Duty Cycle Cluster Env** | Train Envelope | Number of waveform cycles each grain sweeps through |
| **AM LFO / FM LFO** | LFO | Per-pulsaret internal modulation phased by grain windowPhase |
| **BPM Env** | Main | Internal BPM curve, latch-advanced |
| **Pg Waveform** | Main | Single-cycle pulsaret waveform (hand-drawn or up to 32 loaded wav files) |

**Timeline de-locking**: the amp/fm/pan/cluster envelopes each advance at **irrational rate ratios** from the golden-ratio family (1, 1/φ, √(1/φ), √φ) — the envelopes' loop periods never form integer ratios, so the combined state virtually never repeats and the sound keeps evolving along the envelopes instead of settling into a fixed loop. **Envelope Scan Speed** globally scales the envelope timeline rate.

---

## Signal Flow (audio thread)

```
processBlock
  → PulsarSynth::renderNextBlock
       → PulsarSynthVoice::processBlockSamples
            → per-sample processSample()
                 ├── Train state machine: Pulse / IntraSilence / InterTrainSilence
                 ├── changeStage → maskGatedSpawn()
                 │     ├── mask gating (Burst / Euclid / Stochastic)
                 │     ├── latch amp / fm / pan / cluster from the envelopes
                 │     └── spawnGrain: fundamental + harmonic layers + unison pair
                 └── GranularEngine::renderSample
                       └── all active grains: wavetable lookup × window × ADSR × AM/FM LFO
                           → per-grain equal-power pan → (L, R)
            → pulseBuffer → clamp + fast tanh soft clip × output gain → main buffer
```

### Presets & State

- Parameters go through the **APVTS**; mask text and similar go through **ValueTree Properties**.
- `getStateInformation` / `setStateInformation` save XML; loading a preset broadcasts UI updates.

---

## Parameter Overview (APVTS)

| Parameter | Range | Description |
|-----------|-------|-------------|
| Output Gain | -60 ~ +6 dB | Output level |
| Train Period | 1 ~ 16 | Train length (beat blocks) |
| Train Duty Cycle | 1 ~ 320 | Pulsar periods in the sounding section |
| Train Silence | 0 ~ 64 | Silent periods between trains |
| Mask Mode | Off / Burst / Euclid / Stochastic | Rhythmic mask |
| Euclid Steps / Hits | 1 ~ 16 | Euclidean rhythm |
| AM / FM Env Amount | 0 ~ 1 | Envelope modulation amount |
| Pan Amount | 0.1 ~ 1 | How far the pan envelope deviates from center |
| Duty Cycle Cluster Amount | 0.5 ~ 1 | Cluster envelope modulation amount |
| AM / FM LFO Amount | 0 ~ 1 | Per-pulsaret LFO modulation amount |
| Attack / Decay / Sustain / Release | 0 ~ 1 | Grain ADSR window (lifetime fractions / level) |
| Harmonics | 0 ~ 1 | Harmonic pulsar layer level (0 = off) |
| Unison Detune | 0 ~ 50 cents | Detuned unison pair (0 = off) |
| Wave Scan Speed | 0 ~ 5 | Wavetable frame scan rate |
| Envelope Scan Speed | 0 ~ 15 | Modulation envelope timeline rate |

## UI

Three pages (switched by the top buttons):

- **Main**: train structure (Period / Duty Cycle / Silence), mask, ADSR, Harmonic / Detune / Wave Scan Speed / Env Scan Speed knobs, BPM envelope, Pg Waveform canvas (hand-drawn or up to 32 loaded wav files), output waveform display.
- **LFO**: AM LFO / FM LFO canvases + Amount.
- **Train Envelope**: the AM / FM / Pan / Duty Cycle Cluster envelope canvases, each with Min / Max / Scale / Amount.

---

## Tools (tools/)

### generate_waveforms.py

Single-cycle waveform generator: each waveform is **9600 samples / 96 kHz** wav header, peak-normalized, edge-smoothed — ready to load into the Pg Waveform wavetable. Covers 100+ waveform families and 300+ waveforms: band-limited classics, pulsaret grain shapes (gauss/FOF/VOSIM/sinc/expodec…), FM / phase distortion / wavefolding, inharmonic metallics, fractal chaos, geometric curves, a noise-free complex-shape expansion set, and more.

```bash
# Each run draws random parameters for a fresh set of variants (prints a reproducible seed)
python3 tools/generate_waveforms.py [output_dir]

# Reproduce a specific set
python3 tools/generate_waveforms.py --seed 12345

# The classic set with fixed parameters
python3 tools/generate_waveforms.py --fixed
```

Dependency: `numpy`. There is also `generate_pulsarets.py` for generating pulsaret material.

---

## Building

CMake ≥ 3.28, C++20. The JUCE path can be set via the environment / CMake variable `PULSAR_JUCE_DIR` (defaults: `/Applications/JUCE` on macOS, `E:/env/juce-7.0.9600-windows/JUCE` on Windows).

### macOS

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build          # AU + VST3 + Standalone
```

### Windows (VS 2022)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DPULSAR_JUCE_DIR="E:/env/juce-7.0.9600-windows/JUCE"
cmake --build build --config Release   # VST3
```

- `COPY_PLUGIN_AFTER_BUILD TRUE`: the plugin is copied to the system plugin directory after building (disable this option if you hit permission errors and grab the artefacts from `build/pulsar_synth_artefacts/` directly).
- Artefacts: `build/pulsar_synth_artefacts/[Release/]VST3/Pulsar Wang.vst3`, `Standalone/Pulsar Wang.app|.exe`.

## Running

- **Standalone**: open the `Pulsar Wang` app directly.
- **DAW**: load `Pulsar Wang.vst3` (or the macOS AU); the plugin is a synthesizer (exposes a MIDI/event input bus).

---

## API Docs

```bash
doxygen Doxyfile   # output: docs/html/
```

## License

Add your project license here. Using JUCE requires compliance with the [JUCE license terms](https://juce.com/juce-legal) (GPL or commercial, etc.).
