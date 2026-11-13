#!/usr/bin/env python3
"""Generate PixelPressbox's default sound effects (original synth sounds, free to use).
Writes 16-bit mono WAVs into sdcard/sounds/. Replace any of them with your own .mp3/.wav,
or add team-specific ones like LV_td.mp3 / LAD_run.mp3 (team sound wins over the default)."""
import os
import wave

import numpy as np

SR = 22050
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sdcard", "sounds")


def note(freq, dur, kind="brass", vol=0.5):
    t = np.arange(int(SR * dur)) / SR
    if kind == "brass":   # bright, horn-like: many harmonics + a little vibrato
        vib = 1 + 0.004 * np.sin(2 * np.pi * 5.5 * t)
        w = sum((1 / k) * np.sin(2 * np.pi * freq * k * vib * t) for k in range(1, 8))
        env = np.minimum(1, t / 0.03) * np.exp(-t * 1.2)
    elif kind == "organ":  # stadium-organ-ish: drawbar harmonics
        w = sum(a * np.sin(2 * np.pi * freq * m * t) for m, a in ((0.5, .5), (1, 1), (2, .6), (3, .3), (4, .25)))
        env = np.minimum(1, t / 0.01) * np.minimum(1, (dur - t) / 0.03)
    elif kind == "bell":
        w = sum(a * np.sin(2 * np.pi * freq * m * t) for m, a in ((1, 1), (2.76, .4), (5.4, .2)))
        env = np.exp(-t * 3.5)
    else:  # blip
        w = np.sign(np.sin(2 * np.pi * freq * t)) * 0.4
        env = np.exp(-t * 18)
    return vol * w / np.max(np.abs(w)) * env


def crowd(dur, vol=0.18):
    n = np.random.default_rng(7).normal(0, 1, int(SR * dur))
    # rough band-pass so it sounds like a roar, not hiss
    k = np.ones(40) / 40
    n = np.convolve(n, k, mode="same") - np.convolve(n, np.ones(400) / 400, mode="same")
    t = np.arange(len(n)) / SR
    env = np.minimum(1, t / 0.6) * np.minimum(1, (dur - t) / 0.8)
    return vol * n / np.max(np.abs(n)) * env


def seq(parts, gap=0.0):
    out = []
    for p in parts:
        out.append(p)
        if gap:
            out.append(np.zeros(int(SR * gap)))
    return np.concatenate(out)


def mix(*tracks):
    n = max(len(x) for x in tracks)
    m = np.zeros(n)
    for x in tracks:
        m[:len(x)] += x
    return m


def save(name, x):
    os.makedirs(OUT, exist_ok=True)
    x = x / max(1e-9, np.max(np.abs(x))) * 0.9
    pcm = (x * 32767).astype(np.int16)
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print("wrote", name + ".wav", "%.1fs" % (len(x) / SR))


C4, E4, G4, C5, E5, G5, C6 = 261.6, 329.6, 392.0, 523.3, 659.3, 784.0, 1046.5
F4, A4, D5, B4 = 349.2, 440.0, 587.3, 493.9

# Touchdown: rising brass fanfare + crowd swell
fan = seq([note(G4, .16), note(C5, .16), note(E5, .16), note(G5, .5),
           note(E5, .14), note(G5, .9)], gap=0.02)
save("td", mix(fan, crowd(len(fan) / SR + 0.6)))
# Field goal: two-note "it's good" bell + short cheer
fg = seq([note(G5, .45, "bell"), note(C6, .9, "bell")])
save("fg", mix(fg, crowd(1.6, 0.12)))
# Extra point: quick blip-blip
save("xp", seq([note(C6, .09, "blip"), note(G5 * 2, .14, "blip")], gap=0.03))
# Safety / 2-pt: low brass hit
save("safety", seq([note(C4, .25), note(G4, .5)], gap=0.02))
# Run scored (baseball): organ arpeggio climb
save("run", seq([note(f, .14, "organ", .5) for f in (C4, E4, G4, C5)] + [note(E5, .5, "organ", .5)]))
# Generic score (NBA etc., off by default)
save("score", note(E5, .4, "bell"))
# Startup chime
save("startup", seq([note(C5, .12, "bell"), note(E5, .12, "bell"), note(G5, .5, "bell")]))
# Final / win (optional, used if present)
save("win", mix(seq([note(C5, .2), note(E5, .2), note(G5, .2), note(C6, .9)], gap=0.02), crowd(2.2, 0.15)))
# desk clock: soft two-tone chime before a meeting, rising three-note bell when a focus timer ends
save("chime", seq([note(G5, .35, "bell", .45), note(E5, .7, "bell", .45)], gap=0.05))
save("focus", seq([note(C5, .18, "bell", .45), note(G5, .18, "bell", .45), note(C6, .6, "bell", .45)]))
