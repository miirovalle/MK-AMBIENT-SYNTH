# MK RATTUS TRANCE GATE

**Licence: GPL-3.0** — see [LICENSE](LICENSE). This project links the GPL-3.0
[Gate12](https://github.com/tiagolr/gate12) curve engine and is therefore distributed under the
same licence; the upstream copyright and attribution are documented under
[Gate12 integration and licensing](#gate12-integration-and-licensing) below.

JUCE-based rhythmic gating audio effect built on the Gate12 curve engine. It creatively snips, cuts and filters incoming audio into rhythmic patterns: classic staccato trance gates, sidechain-style pumping, stereo alternating/ping-pong gating and a gate-swept 12 dB/oct lowpass filter gate.

This is the standalone effect version of the trance gate inside MK NOISE RhYTM Machine. Insert it on any audio track; it gates whatever audio passes through it.

## Controls

- **Gate On** bypasses the gate while keeping the plugin inserted. **Randomize Gate** generates a fresh editable pattern.
- **Preset menu**: 130 authored presets. Selecting one sets the curve, point count, cycle, smoothing, filter and stereo mode. A short description appears bottom-right. The preset interpretations are documented in the MK NOISE RhYTM Machine README; they are original musical interpretations of their names.
- **Cycle** sets the envelope length (2 bars down to 1/32, plus triplet/dotted and beat-fraction lengths). **Shape** selects the Gate12 segment type; Curve with zero tension is linear.
- **MIDI Retrigger** restarts the gate at each incoming MIDI note-on; off follows host song position including seeks. Without host timing the gate runs at 120 BPM.
- **Curve grid**: drag the 8–32 points to edit level and time. Interior points move freely without crossing neighbors; endpoints stay at the loop boundaries. Changing the point count subdivides or simplifies the current curve.
- **Swing**: 150 px vertical slider left of the grid. Delays offbeat sixteenth notes from straight (0%) to 2:1 triplet feel (100%); eighth-note boundaries stay on time.
- **Attack / Release** smooth gate transitions. **Curve Tension** shapes the selected segment type.
- **Mix** blends dry and gated signal for parallel gating; 0% is an exact dry pass-through. **Output** trims the gated signal before the mix.
- **Filter Gate** sweeps a real two-pole (12 dB/oct) lowpass with the gate envelope in addition to amplitude gating. **Linked / Alternating / Ping Pong** control stereo gating (the right channel gates a half-cycle offset).

All settings and curve edits are saved with the project and are automatable.

## Gate12 integration and licensing

The full upstream [Gate12 source](https://github.com/tiagolr/gate12) is copied under `ThirdParty/Gate12`, pinned to commit `df652453236fa03859dbd5fb8eb3dde1fb574ab5` (version 1.3.3). The plugin compiles and directly calls its original `Pattern` curve functions.

Gate12 is copyright tiagolr/tilr and licensed under GPLv3; see `ThirdParty/Gate12/LICENSE`. Its GPLv3 requirements apply to this Gate12-derived integration: preserve the upstream notices and provide the corresponding source and license when distributing binaries. The VST3 build includes the Gate12 license in its Resources folder. Distribution also requires an appropriate JUCE license or compliance with JUCE's open-source terms.

Local modifications to Gate12: `Pattern.cpp` includes `Globals.h` instead of the unrelated desktop plugin processor; `Pattern.h` explicitly includes `<cstdint>`; point/version counters use atomics. The curve algorithms are unchanged. The integration uses fixed parameter snapshots and calls segment functions directly, avoiding the upstream `get_y_at` mutex on the audio thread.

Only `ThirdParty/Gate12/src/dsp/Pattern.cpp` is compiled; the rest of the upstream tree is kept verbatim so the pinned version stays reviewable. Upstream assets that ship with it (the `doc/` images and the `resources/Ubuntu-Medium.ttf` typeface, under the Ubuntu Font Licence) are not compiled into this plug-in.

## Build

Windows (VST3 and Standalone):

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target MKRattusTranceGate_VST3 MKRattusTranceGate_Standalone
```

On macOS, CMake also configures VST3, AUv3 and Standalone targets. For iOS/iPadOS the iOS formats are `AUv3 Standalone`, so the appex is wrapped in an installable app; CI builds the AUv3 + IPA on every push via [`.github/workflows/ios.yml`](../.github/workflows/ios.yml). AUv3 requires Apple's Xcode toolchain and cannot be built on Windows:

```sh
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-ios --config Release --target MKRattusTranceGate_AUv3
```

The AUv3 is an iOS app extension (AU type `kAudioUnitType_MusicEffect`, so it accepts MIDI retrigger input); packaging it into an installable iPad app requires an Xcode host app and code signing.

The project uses the JUCE checkout in the parent `JUCE` folder when available; otherwise CMake fetches JUCE 9.0.5. Set `MK_JUCE_SOURCE_DIR` to use a different local JUCE checkout.

Build `MKRattusTranceGateTests`, then run `ctest --test-dir build -C Release --output-on-failure`. Tests cover gate timing/tempo/seek/retrigger, bypass and mix passthrough, sixteenth-note swing, stereo and filter gating, every preset's applied curve, state restore, and editor layout plus menu-driven preset selection.
