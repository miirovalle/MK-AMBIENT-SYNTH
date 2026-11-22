#!/usr/bin/env python3
"""Spectral / spatial analysis of the offline renders.

Reads the 32-bit float WAV files written by mk_render and the presets.csv
manifest, then reports objective measurements for every factory program:
level, headroom, stereo width, spectral slope, decay time, plus dedicated
checks for the noise colours, aliasing and unison width.
"""

from __future__ import annotations

import csv
import os
import struct
import sys

import numpy as np

SR = 48000
NOTE_OFF_SECONDS = 5.0


def read_wav_float(path: str) -> np.ndarray:
    """Returns a (frames, 2) float array from a 32-bit float WAV file."""
    with open(path, "rb") as handle:
        data = handle.read()

    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a RIFF/WAVE file")

    pos = 12
    fmt = None
    while pos + 8 <= len(data):
        chunk_id = data[pos : pos + 4]
        chunk_size = struct.unpack("<I", data[pos + 4 : pos + 8])[0]
        body = pos + 8
        if chunk_id == b"fmt ":
            fmt = struct.unpack("<HHIIHH", data[body : body + 16])
        elif chunk_id == b"data":
            audio_format, channels, _, _, _, bits = fmt
            if audio_format != 3 or bits != 32:
                raise ValueError(f"{path}: expected 32-bit float, got format {audio_format}/{bits}")
            frames = chunk_size // (channels * 4)
            samples = np.frombuffer(data[body : body + frames * channels * 4], dtype="<f4")
            return samples.reshape(frames, channels).astype(np.float64)
        pos = body + chunk_size + (chunk_size & 1)

    raise ValueError(f"{path}: no data chunk")


def db(value: float) -> float:
    return 20.0 * np.log10(max(value, 1e-12))


def band_energy(signal: np.ndarray, lo: float, hi: float) -> float:
    spectrum = np.abs(np.fft.rfft(signal * np.hanning(len(signal)))) ** 2
    freqs = np.fft.rfftfreq(len(signal), 1.0 / SR)
    mask = (freqs >= lo) & (freqs < hi)
    return float(spectrum[mask].sum())


def spectral_slope_db_per_octave(signal: np.ndarray, fmin: float = 80.0, fmax: float = 12000.0) -> float:
    spectrum = np.abs(np.fft.rfft(signal * np.hanning(len(signal)))) ** 2
    freqs = np.fft.rfftfreq(len(signal), 1.0 / SR)
    mask = (freqs >= fmin) & (freqs <= fmax)
    if not mask.any():
        return 0.0
    xs = np.log2(freqs[mask])
    ys = 10.0 * np.log10(np.maximum(spectrum[mask], 1e-20))
    return float(np.polyfit(xs, ys, 1)[0])


def spectral_centroid(signal: np.ndarray) -> float:
    spectrum = np.abs(np.fft.rfft(signal * np.hanning(len(signal))))
    freqs = np.fft.rfftfreq(len(signal), 1.0 / SR)
    total = spectrum.sum()
    return float((spectrum * freqs).sum() / total) if total > 0 else 0.0


def rt60_from_tail(tail: np.ndarray) -> float:
    block = int(0.05 * SR)
    usable = len(tail) // block
    if usable < 8:
        return 0.0
    energy = np.array([np.sum(tail[i * block : (i + 1) * block] ** 2) for i in range(usable)])
    if energy.max() <= 0:
        return 0.0
    decay_db = 10.0 * np.log10(np.maximum(energy / energy.max(), 1e-12))
    times = np.arange(usable) * 0.05

    mask = (decay_db < -5.0) & (decay_db > -35.0)
    if mask.sum() < 4:
        return 0.0
    slope = np.polyfit(times[mask], decay_db[mask], 1)[0]
    if slope >= -0.1:
        # Decay slower than the measurement window: report "long" instead of
        # extrapolating to an implausible number.
        return 0.0
    rt = -60.0 / slope
    return float(rt) if rt < 180.0 else 0.0


def stereo_stats(audio: np.ndarray) -> dict:
    left, right = audio[:, 0], audio[:, 1]
    mid = 0.5 * (left + right)
    side = 0.5 * (left - right)
    mid_e = float(np.sum(mid**2))
    side_e = float(np.sum(side**2))
    correlation = float(np.sum(left * right) / max(1e-12, np.sqrt(np.sum(left**2) * np.sum(right**2))))
    return {
        "correlation": correlation,
        "width_pct": 100.0 * side_e / max(1e-12, mid_e + side_e),
    }


def analyse_preset(path: str) -> dict:
    audio = read_wav_float(path)
    left, right = audio[:, 0], audio[:, 1]
    mono = 0.5 * (left + right)

    release_index = int(NOTE_OFF_SECONDS * SR)
    sustain = mono[:release_index]
    tail = mono[release_index:]

    stats = stereo_stats(audio)
    sustain_energy = band_energy(sustain, 20.0, 20000.0)
    stats.update(
        {
            "peak": float(np.max(np.abs(audio))),
            "peak_db": db(float(np.max(np.abs(audio)))),
            "rms": float(np.sqrt(np.mean(audio**2))),
            "rms_db": db(float(np.sqrt(np.mean(audio**2)))),
            "crest_db": db(float(np.max(np.abs(audio)) / max(1e-9, np.sqrt(np.mean(audio**2))))),
            "dc": float(np.mean(mono)),
            "slope": spectral_slope_db_per_octave(sustain),
            "centroid": spectral_centroid(sustain),
            "low_pct": 100.0 * band_energy(sustain, 20.0, 120.0) / max(1e-12, sustain_energy),
            "air_pct": 100.0 * band_energy(sustain, 8000.0, 20000.0) / max(1e-12, sustain_energy),
            "rt60": rt60_from_tail(tail),
            "tail_end_db": db(float(np.sqrt(np.mean(tail[-int(1.0 * SR) :] ** 2)))),
            "sustain_db": db(float(np.sqrt(np.mean(sustain**2)))),
            "finite": bool(np.all(np.isfinite(audio))),
        }
    )
    return stats


def analyse_noise(path: str) -> float:
    audio = read_wav_float(path)
    return spectral_slope_db_per_octave(audio[int(1.0 * SR) : int(4.0 * SR), 0], 100.0, 12000.0)


def analyse_alias(path: str, note: int) -> dict:
    audio = read_wav_float(path)
    left = audio[int(0.5 * SR) : int(1.5 * SR), 0]
    f0 = 440.0 * 2.0 ** ((note - 69) / 12.0)

    window = left * np.hanning(len(left))
    spectrum = np.abs(np.fft.rfft(window))
    freqs = np.fft.rfftfreq(len(left), 1.0 / SR)

    threshold = spectrum.max() * 0.02
    peaks = []
    for i in range(2, len(spectrum) - 2):
        if (
            spectrum[i] > threshold
            and spectrum[i] >= spectrum[i - 1]
            and spectrum[i] >= spectrum[i + 1]
            and freqs[i] < 15000.0
        ):
            peaks.append(freqs[i])

    errors = [abs(freq / f0 - round(freq / f0)) for freq in peaks[:40]]
    above_19k = float(np.sum(spectrum[freqs > 19000.0] ** 2) / max(1e-20, np.sum(spectrum**2)))
    return {
        "f0": f0,
        "num_peaks": len(peaks),
        "worst_harmonic_error": max(errors) if errors else 0.0,
        "energy_above_19k": above_19k,
    }


def load_manifest(directory: str) -> list:
    path = os.path.join(directory, "presets.csv")
    if not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def gate_cycles(path: str, min_rate: float = 0.4, max_rate: float = 40.0) -> tuple:
    """Measures the gate's modulation rate from the rendered audio.

    Returns (cycles_per_second, depth_db) using a smoothed RMS envelope and
    mean-crossing counting.
    """
    audio = read_wav_float(path)
    mono = 0.5 * (audio[:, 0] + audio[:, 1])

    window = int(0.010 * SR)
    count = len(mono) // window
    envelope = np.array([np.sqrt(np.mean(mono[i * window : (i + 1) * window] ** 2)) for i in range(count)])

    # Smooth over 3 windows (30 ms) so the gate shape survives but the
    # waveform itself does not create crossings.
    kernel = np.ones(3) / 3.0
    smooth = np.convolve(envelope, kernel, mode="same")[3:-3]

    peak = float(smooth.max())
    floor = float(np.percentile(smooth, 5))
    level = 0.5 * (peak + floor)
    hysteresis = 0.15 * (peak - floor)

    crossings = 0
    state = 0
    for value in smooth:
        if state <= 0 and value > level + hysteresis:
            crossings += 1
            state = 1
        elif state >= 0 and value < level - hysteresis:
            crossings += 1
            state = -1

    duration = len(smooth) * 0.010
    return crossings / 2.0 / max(1e-6, duration), db(peak) - db(max(floor, 1e-9))


def main() -> int:
    directory = sys.argv[1] if len(sys.argv) > 1 else "build/renders"
    manifest = load_manifest(directory)

    if not manifest:
        print(f"no presets.csv in {directory}")
        return 1

    print("MK Ambient - preset bank analysis")
    print("=" * 112)

    rows = []
    failures = []

    for entry in manifest:
        path = os.path.join(directory, entry["filename"])
        if not os.path.exists(path):
            failures.append((entry["name"], "missing render"))
            continue

        stats = analyse_preset(path)
        row = {
            "index": int(entry["index"]),
            "name": entry["name"],
            "category": entry["category"],
            **stats,
        }
        rows.append(row)

        checks = [
            (stats["finite"], "NaN/inf"),
            (stats["peak"] <= 1.0, f"clipping ({stats['peak']:.3f})"),
            (stats["rms_db"] > -45.0, f"too quiet ({stats['rms_db']:.1f} dB)"),
            (abs(stats["dc"]) < 0.01, "DC offset"),
            (stats["width_pct"] > 3.0, f"too narrow ({stats['width_pct']:.1f}%)"),
            # A decay slower than the 7 s tail window is a very long ambient
            # tail, not a short one: only flag a fitted RT60 that is too short.
            (stats["rt60"] == 0.0 or stats["rt60"] > 1.0, f"short tail ({stats['rt60']:.1f} s)"),
        ]
        for passed, message in checks:
            if not passed:
                failures.append((entry["name"], message))

    report_path = os.path.join(directory, "preset_report.txt")
    with open(report_path, "w", encoding="utf-8") as handle:
        handle.write(
            f"{'#':>3} {'program':<30}{'peak':>7}{'rms dB':>8}{'crest':>7}{'corr':>7}"
            f"{'width%':>8}{'slope':>8}{'centroid':>10}{'low%':>7}{'air%':>7}{'RT60':>7}{'end dB':>8}\n"
        )
        for row in rows:
            rt_text = f"{row['rt60']:>7.1f}" if row["rt60"] > 0 else f"{'long':>7}"
            handle.write(
                f"{row['index']:>3} {row['name'][:30]:<30}{row['peak']:>7.3f}{row['rms_db']:>8.1f}"
                f"{row['crest_db']:>7.1f}{row['correlation']:>+7.2f}{row['width_pct']:>8.1f}"
                f"{row['slope']:>8.2f}{row['centroid']:>10.0f}{row['low_pct']:>7.1f}"
                f"{row['air_pct']:>7.1f}{rt_text}{row['tail_end_db']:>8.1f}\n"
            )

    categories = {}
    for row in rows:
        categories.setdefault(row["category"], []).append(row)

    print(f"{'category':<18}{'count':>6}{'peak max':>10}{'rms avg':>9}{'rms min':>9}"
          f"{'centroid range':>18}{'width%':>9}{'RT60':>7}")
    print("-" * 112)
    for category, group in categories.items():
        peaks = max(r["peak"] for r in group)
        rms_values = [r["rms_db"] for r in group]
        centroids = [r["centroid"] for r in group]
        widths = [r["width_pct"] for r in group]
        tails = [r["rt60"] for r in group if r["rt60"] > 0]
        tail_text = f"{np.mean(tails):.1f}" if tails else "long"
        print(
            f"{category:<18}{len(group):>6}{peaks:>10.3f}{np.mean(rms_values):>9.1f}"
            f"{np.min(rms_values):>9.1f}"
            f"{f'{np.min(centroids):.0f}-{np.max(centroids):.0f} Hz':>18}"
            f"{np.mean(widths):>9.1f}{tail_text:>7}"
        )

    print("-" * 112)
    print(f"programs analysed: {len(rows)}   failures: {len(failures)}   full table: {report_path}")
    for name, message in failures[:40]:
        print(f"  FAIL {name}: {message}")

    if rows:
        print("-" * 112)
        print("extremes")
        for title, key, reverse in (("brightest", "centroid", True), ("darkest", "centroid", False),
                                    ("quietest", "rms_db", False), ("narrowest", "width_pct", False),
                                    ("widest", "width_pct", True)):
            top = sorted(rows, key=lambda r: r[key], reverse=reverse)[:3]
            formatted = "  ".join(f"{r['name']} ({r[key]:.0f})" for r in top)
            print(f"  {title:<10} {formatted}")

    print("-" * 112)
    print("five flagship programs")
    for row in rows[:5]:
        print(
            f"  {row['name']:<30} peak {row['peak']:.3f}  rms {row['rms_db']:.1f} dB  "
            f"width {row['width_pct']:.0f}%  centroid {row['centroid']:.0f} Hz  RT60 {row['rt60']:.1f} s"
        )

    targets = {
        "white": (-0.8, 1.2),
        "pink": (-4.2, -2.0),
        "brown": (-8.5, -4.6),
        "blue": (1.8, 4.6),
        "violet": (4.6, 8.0),
        "grey": (-2.5, 2.5),
    }
    print("-" * 112)
    print("noise spectra (dB/octave; white 0, pink -3, brown -6, blue +3, violet +6)")
    slopes = {}
    for colour, (lo, hi) in targets.items():
        path = os.path.join(directory, f"noise_{colour}.wav")
        if not os.path.exists(path):
            print(f"  {colour:<8} MISSING")
            failures.append((colour, "missing noise render"))
            continue
        slope = analyse_noise(path)
        slopes[colour] = slope
        status = "ok" if lo <= slope <= hi else "FAIL"
        if status == "FAIL":
            failures.append((colour, f"noise slope {slope:+.2f}"))
        print(f"  {colour:<8} {slope:+6.2f} dB/oct   expected {lo:+.1f}..{hi:+.1f}   {status}")

    if all(c in slopes for c in ("brown", "pink", "white", "blue", "violet")):
        ordered = slopes["brown"] < slopes["pink"] < slopes["white"] < slopes["blue"] < slopes["violet"]
        print(f"  ordering brown < pink < white < blue < violet: {'ok' if ordered else 'FAIL'}")
        if not ordered:
            failures.append(("noise", "colour ordering"))

    alias_path = os.path.join(directory, "probe_alias_c7.wav")
    if os.path.exists(alias_path):
        alias = analyse_alias(alias_path, 96)
        print("-" * 112)
        print(
            f"aliasing probe C7: f0 {alias['f0']:.1f} Hz, {alias['num_peaks']} peaks, "
            f"worst harmonic error {alias['worst_harmonic_error']:.3f}, "
            f"energy above 19 kHz {100 * alias['energy_above_19k']:.4f}%"
        )
        if alias["worst_harmonic_error"] > 0.06:
            failures.append(("aliasing", "non-harmonic peaks"))
        if alias["energy_above_19k"] > 0.002:
            failures.append(("aliasing", "ultrasonic content"))

    narrow_path = os.path.join(directory, "probe_width_narrow.wav")
    wide_path = os.path.join(directory, "probe_width_wide.wav")
    if os.path.exists(narrow_path) and os.path.exists(wide_path):
        n = stereo_stats(read_wav_float(narrow_path))
        w = stereo_stats(read_wav_float(wide_path))
        print(
            f"width probe: centred {n['correlation']:+.3f} corr / {n['width_pct']:.1f}% side, "
            f"spread {w['correlation']:+.3f} corr / {w['width_pct']:.1f}% side"
        )
        if n["correlation"] < 0.99:
            failures.append(("width", "centred unison is not mono"))
        if w["correlation"] > 0.9:
            failures.append(("width", "spread unison is not wide"))

    # ---- trance gate --------------------------------------------------------
    gate_paths = [
        ("probe_gate.wav", 2.0, "1/4 at 120 BPM"),
        ("probe_gate_fast.wav", 4.0, "1/8 at 120 BPM"),
    ]
    gate_results = []
    for filename, expected, label in gate_paths:
        path = os.path.join(directory, filename)
        if not os.path.exists(path):
            print(f"  {filename:<22} MISSING")
            failures.append(("gate", f"missing {filename}"))
            continue
        rate, depth = gate_cycles(path)
        gate_results.append((label, expected, rate, depth))
        ok = abs(rate - expected) <= max(0.35, expected * 0.12) and depth > 12.0
        print(f"  {label:<20} {rate:5.2f} Hz (expect {expected:.1f})   depth {depth:5.1f} dB   {'ok' if ok else 'FAIL'}")
        if not ok:
            failures.append(("gate", f"{label} rate {rate:.2f} Hz depth {depth:.1f} dB"))

    print("=" * 112)
    print("RESULT:", "PASS" if not failures else f"FAIL ({len(failures)} issues)")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
