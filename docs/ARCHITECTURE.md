# MK Ambient — architecture

## Design goals

1. **iPad-first.** Every processing block is allocation-free, modulation runs at control rate,
   and the expensive tables/samples are built once outside the audio callback.
2. **Portable core.** All synthesis lives in `Source/DSP` with no framework dependency, so it can
   be unit-tested and rendered offline on any machine. JUCE only wraps it for AUv3/VST3 and
   draws the UI.
3. **One source of truth for parameters.** A single X-macro table defines the parameter ids,
   ranges, defaults, units, choice lists and grouping. The registry, the JUCE
   `AudioProcessorValueTreeState`, presets and the UI are all derived from it.
4. **Extensible by composition.** A "voice" is oscillators + filter + envelopes; a "slot" is a
   source + filter + LFO + pan + sends. Adding a layer, an LFO destination or a new texture
   family is a local change.

## Module map

```
Source/DSP
  MkCommon.h        math, PRNG (seeded xorshift), pan, smoothing, sine table
  Params.h/.cpp     180-parameter table, registry, choice lists, slot/LFO block helpers
  ParamStore.h/.cpp atomic targets + audio-thread smoothing + snapshot
  Filters.h/.cpp    TPT state-variable filter, multimode voice filter, one-pole, biquad,
                    power-of-two delay line with Hermite reads, all-pass diffuser
  Wavetables.h/.cpp 6 wave sets x 8 morph frames x 8 band-limited mips, additive build
  Oscillator.h      7-voice unison oscillator: detune, stereo spread, analog drift, mip pick
  Envelope.h        curved ADSR with retrigger-safe attack and shaped release table
  Lfo.h             7 shapes incl. sample & hold and smooth random, control-rate advance
  NoiseGen.h/.cpp   white / pink / brown / blue / violet / grey generators
  SampleLibrary.*   static catalogue (132 entries, 6 categories) + deterministic generation
  SamplePlayer.*    seamless crossfaded loop, one-shot, 4-grain granular cloud
  Effects.h/.cpp    saturator (stereo-safe), ensemble, free-time stereo delay, 8-line FDN
                    reverb, look-ahead limiter
  Voice.h/.cpp      pad voice: 2 unison oscillators, stereo filter, two envelopes, M/S pan
  TranceGate.*      12-step tempo-synced gate: step levels, attack/release ramps, tension
                    curve, Min/Max window, phase, Sync/Free/Note, shared preview maths
  NoiseEngine.*     4 slots, per-slot chain, per-slot LFO and sends
  SynthEngine.*     buses, mod matrix, voice allocation, FX routing, master, metering
  Presets.h/.cpp    20 designed families x 5 moods = 100 programs (+ Init), flagship first

Source/UI
  Theme.*           olive-gray palette, panel drawing, popup look-and-feel
  Knob.*            Knob (custom gesture handling), ChoiceButton (grouped menus), Toggle,
                    Meter, SimpleButton
  Keyboard.*        on-screen piano keyboard: multi-touch, glissando, 7-octave scroll bar
  GateEditor.*      step sequencer widget: pattern strip, paint/point drag, snap, undo/redo,
                    playhead, DSP-accurate preview curve
  Panels.*          Card grid container and the six pages

Source/Plugin
  PluginProcessor.* APVTS bridge, MIDI/sustain handling, keyboard FIFO, programs, state
  PluginEditor.*    scaled 4:3 canvas: header, tab bar, pages, keyboard, metering

Tests/              assertion suite (registry, filters, generators, effects, engine)
Tools/              offline renderer + numpy analysis
```

## Signal flow

```
MIDI note on/off ──► SynthEngine::noteOn ──► VoiceAllocator (16 voices, oldest/quietest steal)
                                               │
                        ┌──────────────────────┴──────────────────────┐
                        ▼                                             ▼
              pad voices (2 osc, filter, env)              4 texture slots
                        │                                             │
              padL/R + pad space send                    dry L/R + delay send + space send
                        │                                             │
                   ensemble (M/S width)                               │
                        └───────────────┬─────────────────────────────┘
                                        ▼
                    sum ──► master width ──► saturation ──► low cut ──► limiter ──► out
                    ▲                                        ▲
        delay return (HP 220 Hz) ──► reverb send ──► reverb return (HP 160 Hz)
```

* The pad bus and the texture bus are summed pre-master; the delay and reverb are parallel
  returns whose wet levels are the `Mix` parameters.
* Both returns are high-passed (220 Hz / 160 Hz) so long tails never muddy the low end.

## Control-rate architecture

`SynthEngine::process` splits the host block into sub-blocks of at most 32 samples
(`kControlBlockSize`). For each sub-block it:

1. advances the three global LFOs by `n / sampleRate`,
2. resolves the whole modulation matrix into a `VoiceControls` struct,
3. renders voices and texture slots into pre-allocated buses.

Voices update oscillator increments, mip levels, filter coefficients and M/S pan at this rate;
only envelopes and oscillators run per sample. This is the key CPU saving on iPad: filter
coefficient trigonometry and table-mip selection happen ~1500 times per second instead of 48 000.

Parameters use `ParamStore`: atomics written by the host/UI thread, one-pole smoothing advanced
once per block on the audio thread. Preset loads only write targets, so no message-thread code
ever touches audio-thread state.

## Real-time rules

* All buffers are allocated in `prepare()` (`padL/R`, sends, wet returns, delay lines, scratch).
* `process()` performs no allocation, no locks, no file or network I/O, no `std::string`.
* Denormals are flushed in feedback paths; the host also runs with `ScopedNoDenormals`.
* Sample/wavetable generation is one-time work (~130 ms) done inside `prepare`, with a
  deterministic seed so results are identical on every machine.

## Wavetables and anti-aliasing

Each wave set contains eight morph frames. Every frame is generated additively as a sum of
sine harmonics whose amplitude comes from the set's spectral model (analog saw, warm pad, hollow,
glass, formant, organ). Eight mip levels hold 256, 128, 64 … 2 harmonics; the oscillator picks
the level from its phase increment, so harmonics never fold above Nyquist. A C7 saw measures
0.000 harmonic error and no energy above 19 kHz.

## Texture library

`sampleCatalog()` is a static table of 132 descriptors (name, family, seed, duration). The audio
is rendered at 24 kHz mono with seeded recipes per family:

| family | character | recipe |
|---|---|---|
| Crackle | dust, static, pops | sparse decaying sine + noise bursts, brown rumble bed |
| Vinyl | surface noise, groove | filtered pink bed with wow, crackle, rumble |
| Tape | hiss, dropouts | band-limited hiss with wow/flutter and gain dips |
| Foley | knocks, ticks, rustles, hits | transient bursts with resonant bodies |
| Granular | icy blips, sparkles | sparse pitched decaying sines + air bed |
| City | traffic, tunnels, rain | shaped rumble + AM traffic band + distant tones |

Storage is 24 kHz 16-bit (≈7.1 MB) and playback rate-corrects against the host rate, so memory
stays modest on iPad while preserving all the character these textures need. The 16-bit form
halves the footprint versus float without adding work on the play path: samples are read as
`int16_t` and scaled by `SampleView::scale` in the same multiply that applies the level.

## Reverb

An 8-line feedback delay network: pre-delay → 2 all-passes of diffusion per channel → 8 mutually
prime delay lines → per-line damping and RT60-derived gain → orthonormal 8-point
Walsh-Hadamard mixing → alternating-channel injection. Output is taken from even/odd line pairs,
converted to M/S for the width control and high-passed at 160 Hz. Line lengths scale with
`Space Size`, gains derive from `Decay` (0.9–32 s RT60), and slow per-line modulation removes
metallic ringing.

## Voice

* Two `UnisonOscillator`s; oscillator B is tuned by the interval table (−24 … +12 semitones).
* Unison: symmetric detune across the stack, equal-power pan per voice scaled by `Spread`, slow
  random drift per unison voice (`Drift`), loudness normalised by `1/sqrt(n)`.
* One stereo `MultimodeFilter` (LP12/LP24/BP/HP), cutoff = base × key-track × 2^(env×amount×4
  octaves + LFO semitones), optional soft drive inside the filter.
* Curved ADSR for amplitude and a second ADSR for the filter.
* Pan is applied in mid/side, so a centred pan is bit-exact identity and LFO panning moves the
  image without collapsing it.

## Noise slot chain

`source → per-slot filter (LP/BP/HP, LFO on cutoff) → soft drive + DC block → swell envelope →
pan (LFO) → dry bus + delay send + space send`. Sources can be procedural colours or catalogue
samples in loop, one-shot or grain mode; the grain cloud runs four Hann-windowed grains with
position spray, pitch jitter and a slow drift through the sample.

## Trance gate

`TranceGate` is a per-sample amplitude sequencer driven at control rate:

* **Timing** — `update()` converts the chosen pattern length (2 bars … 1/16) plus host BPM into
  samples per step (`beats × 60 / bpm × sr / 12`), or `sr / (rate × 12)` in Free/Note mode. The
  engine calls `setTempo()` from the host playhead every block; without a playhead it stays at
  120 BPM.
* **Per sample** — `nextGain()` advances `stepPosition` (measured in steps, so one step lasts
  `stepSamples` samples), and on a step change starts a ramp whose length is the attack (rising)
  or release (falling) time, clamped to one step so ramps never overlap. Tension bends the ramp
  with `t^exp2(-tension × 1.6)`.
* **Mapping** — `level = min + (max − min) × step`, then `gain = 1 − mix × (1 − level)`, so
  `mix = 0` is a bit-exact bypass and the gate is cheap when unused (the engine skips the loop).
* **Preview** — `gateEnvelopeAt()` evaluates the same maths as a pure function. `GateEditor`
  samples it across the plot, so the drawn curve is the audio, verified to 0.011 in the tests.
* **Routing** — the gate multiplies the pad bus (including its space send) and/or the texture
  bus and its sends, before the delay and reverb returns, so tails still ring after a chop.

## UI structure

`PluginEditor` owns the header (title, program display, previous/next, output meter), the tab
strip and the pages. The interface is laid out exactly once in **design coordinates**
(1200 × 900, a 4:3 iPad screen) on a `Canvas` child; `resized()` scales that canvas with an
`AffineTransform` to whatever space the window or the AUv3 host provides and centres it. A
small window, Split View or a full iPad therefore show *the same* complete interface, never a
clipped corner, and every control keeps its proportions and touch size relative to the screen.

`Card` lays out its choice buttons and knobs in a grid, so a page only declares the grouping.
Knobs implement their own drag (210 px full travel, ×0.22 with shift), double-tap reset and
wheel support, and talk to the host through `juce::ParameterAttachment` (proper gesture
begin/end for automation). The grid never lets a knob row overflow its cell, so short cards
shrink their knobs instead of overlapping.

`Card::addToggle` puts an on/off switch in the card header. Each switch is a normal float
parameter of unit `U_Switch` (0..1, default 1), so it is automatable, saved with the patch and
smoothed by `ParamStore` over 15 ms — switching a section fades rather than clicks. The DSP
multiplies the section's own gain by the switch value, so: `OscAOn`/`OscBOn` scale the
oscillator levels, `Lfo1On…Lfo3On` scale the LFO output, `Slot1On…Slot4On` scale the slot level,
`DelayOn`/`ReverbOn` scale the return mixes, `EnsembleOn` scales the ensemble depth and
`GateOn` scales the gate mix. Everything defaults to on, so existing programs are unchanged.

`ChoiceButton` builds grouped popup menus from the parameter's value strings — the 138-entry
source list becomes seven sub-menus (procedural + six texture families).

`Keyboard` is the small piano roll along the bottom of the editor: 73 keys (C1–C7) at 34 px per
white key with a scroll bar whenever the range is wider than the window. Each touch source gets
its own pointer, so chords and glissando work on the iPad; note events are pushed into a
`juce::AbstractFifo` and turned into MIDI by `PluginProcessor` on the audio thread, which keeps
voice state off the message thread. Because it sits outside the pages it stays playable on every
tab.

`GateEditor` draws twelve nodes over a 12-column grid, a DSP-accurate envelope curve, step bars
and a moving playhead (30 Hz from engine atomics). Dragging in Point mode writes one step param
with a change gesture per touch; Paint mode writes every column crossed; Snap quantises to
1/16. A 32-entry undo stack stores step arrays, and the eight pattern shapes plus Flip/Random/
Min/Max all funnel through the same `writeSteps()` path so undo always works.

## Preset bank

`Presets.cpp` stores 100 programs plus `Init`. A `Patch` builder writes parameter values by
name; each family function (`buildSubmerged`, `buildCathedral`, …) produces one complete patch.
For every family, five variations are generated:

1. the patch is built,
2. its texture sources are swapped from the family's pool of six candidates,
3. pad families retune the layer oscillator (−12, 0, +12, +7, −5 semitones),
4. `applyMood` scales cutoff, attack/release, LFO and slot rates, detune, reverb size/decay and
   pre-delay by the variation's mood multipliers (all values are re-clamped by `sanitiseParam`).

The balanced variation of a flagship family keeps the exact original patch and name, so the five
classic presets are bit-identical to the hand-tuned versions. After generation the five classics
are stably partitioned to the front of the list.

`ProgramStore` owns the strings and snapshots; `factoryPreset()` hands out stable `const char*`
into that store. The host sees `Init` + 100 programs; the editor shows them 12 per bank.

## Extension points

* **New parameter** — add one `X(...)` line to `Params.h`; registry, APVTS, UI and preset
  snapshots pick it up. Add it to a card to expose it. A parameter in a slot or LFO block must go
  in the matching offset (see `kSlotStride` / `kLfoStride`), which `static_assert`s enforce.
* **New section switch** — add an `X(...)` line with unit `U_Switch` next to its section's
  parameters, multiply that section's gain by it in the DSP, and call `card.addToggle (id)`.
* **New preset** — add a family builder plus a `Family` row (name, category, description, five
  suffixes, six-source texture pool, build function). That yields five new programs at once.
* **New wavetable family** — add a spectral model in `Wavetables.cpp::harmonicAmplitude` and a
  name in `kWaveChoices`; set `kWaveSetCount`.
* **New texture family** — add a `SampleCategory`, a word list, a recipe and a generator case;
  the catalogue, menus and slot sources update automatically.
* **New LFO destination** — add a parameter to the LFO block, resolve it in
  `SynthEngine::renderBlock`, and use it in `VoiceControls`.
* **New gate pattern** — append a 12-value array to `GateEditor::PatternStrip::shapes`; the strip,
  context labels and taps update automatically.
* **New effect** — add a class in `Effects.*`, a bus and send levels in `SynthEngine`; all
  buffers come from `prepare()`.

## Threading

| thread | responsibility |
|---|---|
| audio | `SynthEngine::process`, parameter smoothing, metering atomics, draining the keyboard FIFO into MIDI |
| message (UI/host) | APVTS edits, program loads (targets only), editor repaint at 30 Hz, pushing on-screen keyboard notes into the FIFO |
| construction | wavetable build + sample generation inside `prepare()` |

## Performance notes for iPad

* 16 voices × 2 oscillators × 7 unison = at most 224 table reads per sample, but the default
  8 voices with 5/3 unison is 128 reads — well within A-series capability.
* Control-rate modulation keeps transcendentals out of the sample loop. The filter envelope is
  advanced once per 32-sample control block (`AdsrEnvelope::advanceSamples`) because its only
  consumer is the cutoff; the amp envelope stays per sample.
* Oscillator phases are 32-bit fixed point, so the per-sample inner loop has no float→int
  conversion and no wrap branch: index and fraction come straight out of the phase word
  (`WaveSet::readCursorFixed`), and uint32 overflow *is* the wrap.
* Wave set / frame / mip resolution happens once per control block per voice
  (`UnisonOscillator::setPitch` caches a `WaveSet::Cursor`), never per sample.
* Filter coefficients are recomputed at control rate, `k` is computed once and shared by both
  SVF stages, and the resonance mapping uses `exp2` instead of `pow`.
* Mip-mapped tables avoid oversampling entirely: no 2×/4× DSP cost for anti-aliasing.
* Texture storage at 24 kHz/16-bit quarters memory versus 48 kHz float.
* Reverb, delay and each noise slot short-circuit to nothing while idle (`isIdle()`), so quiet
  patches cost almost nothing.
* Adaptive polyphony: `SynthEngine` compares the measured block time with the block duration.
  Above 65% load for several blocks it lowers the voice budget (floor 4); below 35% for a while
  it restores it up to the Polyphony parameter. Hysteresis prevents flapping, and the guard can
  only reduce voices below what the patch asks for, never above.
* Benchmarks (`mk_bench`): worst case 16 voices + 4 textures + reverb = 4.6% of a desktop core
  at 48 kHz / 256 samples, 8.6 MB resident content (1.5 MB wavetables + 7.1 MB textures), built
  in ~72 ms. Expect roughly a quarter of desktop scalar throughput on an A10X, i.e. ~18–20% of
  one core with a ~1 ms per-block cost against a 5.3 ms budget.
