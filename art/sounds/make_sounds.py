# FuseTag sounds: synthesized in code and written as WAVs to assets/sounds/ (no outside files, same output every run).
#   python art/sounds/make_sounds.py
# Mono 16-bit 22050 Hz. Effects are short; the music is a 16-second loop in three stems (bass, drums, lead) that the game fades
# in with the "tension" parameter (how close the it player's fuse is to blowing). Events and buses are in assets/sounds.events.
import math
import os
import random
import struct
import wave

RATE = 22050
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "assets", "sounds")
rng = random.Random(11)


def write(name, samples, peak=0.9):
    m = max(1e-9, max(abs(s) for s in samples))
    scale = peak / m
    os.makedirs(OUT, exist_ok=True)
    with wave.open(os.path.join(OUT, name), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))
    print("wrote", name, f"{len(samples) / RATE:.2f}s")


def n_of(t):
    return int(t * RATE)


def env(i, attack=0.003, decay=None):
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a if decay is None else a * math.exp(-t / decay)


def lowpass(s, cutoff):
    rc = 1.0 / (2 * math.pi * cutoff)
    k = (1.0 / RATE) / (rc + 1.0 / RATE)
    out, y = [], 0.0
    for v in s:
        y += k * (v - y)
        out.append(y)
    return out


def highpass(s, cutoff):
    return [a - b for a, b in zip(s, lowpass(s, cutoff))]


def noise(n):
    return [rng.uniform(-1, 1) for _ in range(n)]


def tone(f0, f1, length, shape=math.sin, curve=1.0):
    n = n_of(length)
    p, out = 0.0, []
    for i in range(n):
        f = f0 + (f1 - f0) * (i / n) ** curve
        p += 2 * math.pi * f / RATE
        out.append(shape(p))
    return out


def square(p):
    return 1.0 if math.sin(p) >= 0 else -1.0


def tri(p):
    return 2.0 / math.pi * math.asin(math.sin(p))


def mix(*parts):
    n = max(len(p) for p in parts)
    return [sum(p[i] for p in parts if i < len(p)) for i in range(n)]


def shaped(s, attack, decay, gain=1.0):
    return [v * env(i, attack, decay) * gain for i, v in enumerate(s)]


# ---- Tag: a rubbery thwack and a bright two-note "you're it"
thwack = shaped(lowpass(noise(n_of(0.12)), 2500), 0.001, 0.03)
body = shaped(tone(220, 90, 0.16), 0.001, 0.06)
chime = [v * env(i, 0.002, 0.18) * 0.5 for i, v in enumerate(mix(tone(1320, 1320, 0.35, tri), [0.0] * n_of(0.07) + tone(1760, 1760, 0.3, tri)))]
write("tag.wav", mix(thwack, body, [0.0] * n_of(0.02) + chime))

# ---- Blast: deep boom with crackle
boom = shaped(tone(90, 30, 1.2, curve=0.5), 0.002, 0.35)
roar = shaped(lowpass(noise(n_of(1.2)), 900), 0.004, 0.3, 0.8)
crack = shaped(highpass(noise(n_of(0.5)), 3000), 0.001, 0.08, 0.4)
write("blast.wav", mix(boom, roar, crack), peak=0.95)

# ---- Bomb tick (pitched by the game with the fuse) and danger alarm
write("bomb_tick.wav", shaped(mix(tone(1800, 1800, 0.05, square), shaped(lowpass(noise(n_of(0.02)), 4000), 0.0005, 0.005)), 0.0005, 0.015), peak=0.6)
alarm = []
for k in range(3):
    alarm += shaped(tone(1400, 1400, 0.09, square), 0.002, 0.05, 0.6) + [0.0] * n_of(0.04)
write("alarm.wav", lowpass(alarm, 5000), peak=0.6)
heart = mix(shaped(tone(70, 50, 0.12), 0.003, 0.04), [0.0] * n_of(0.16) + shaped(tone(60, 45, 0.12), 0.003, 0.04, 0.7))
write("heartbeat.wav", heart, peak=0.8)

# ---- Movement
write("jump.wav", shaped(tone(260, 620, 0.14, tri, 0.6), 0.002, 0.07), peak=0.55)
write("land.wav", mix(shaped(tone(120, 60, 0.12), 0.001, 0.04), shaped(lowpass(noise(n_of(0.1)), 1200), 0.001, 0.03, 0.5)), peak=0.6)
for k in range(3):
    step = mix(shaped(lowpass(noise(n_of(0.06)), 900 + k * 250), 0.001, 0.012), shaped(tone(140 + k * 15, 90, 0.05), 0.001, 0.015, 0.4))
    write(f"step_{k + 1}.wav", step, peak=0.35)
write("sprint.wav", shaped(highpass(lowpass(noise(n_of(0.25)), 3500), 600), 0.05, 0.1), peak=0.4)
write("winded.wav", shaped(lowpass(noise(n_of(0.5)), 1400), 0.08, 0.2), peak=0.35)

# ---- Hide, found, shove
write("rustle.wav", [v * (0.5 + 0.5 * math.sin(i / RATE * 45)) for i, v in enumerate(shaped(highpass(lowpass(noise(n_of(0.35)), 4500), 900), 0.02, 0.12))], peak=0.45)
pop = shaped(tone(300, 900, 0.08, tri, 0.5), 0.001, 0.04)
write("found.wav", mix(pop, [0.0] * n_of(0.06) + shaped(tone(1050, 1050, 0.25, tri), 0.002, 0.1, 0.5)), peak=0.7)
write("shove.wav", mix(shaped(highpass(lowpass(noise(n_of(0.22)), 3000), 400), 0.03, 0.06), shaped(tone(180, 110, 0.12), 0.002, 0.05, 0.5)), peak=0.6)
write("shove_hit.wav", mix(shaped(lowpass(noise(n_of(0.1)), 1800), 0.001, 0.025), shaped(tone(160, 70, 0.14), 0.001, 0.05)), peak=0.75)
write("impact.wav", mix(shaped(lowpass(noise(n_of(0.15)), 700), 0.001, 0.04), shaped(tone(95, 60, 0.15), 0.001, 0.05, 0.7)), peak=0.7)

# ---- Items
write("pickup.wav", mix(*[[0.0] * n_of(k * 0.05) + shaped(tone(f, f, 0.18, tri), 0.002, 0.08, 0.5) for k, f in enumerate((880, 1109, 1319, 1760))]), peak=0.6)
write("item_use.wav", mix(shaped(tone(600, 1800, 0.2, tri, 0.4), 0.003, 0.08), shaped(highpass(noise(n_of(0.2)), 5000), 0.003, 0.05, 0.3)), peak=0.6)
write("snare_set.wav", mix(shaped(tone(500, 500, 0.03, square), 0.0005, 0.01), [0.0] * n_of(0.05) + shaped(tone(700, 700, 0.03, square), 0.0005, 0.01)), peak=0.5)
write("snare_snap.wav", mix(shaped(highpass(noise(n_of(0.08)), 2500), 0.0005, 0.015), shaped(tone(900, 300, 0.1, square), 0.0005, 0.03, 0.4)), peak=0.8)
write("crate_back.wav", shaped(tone(1500, 2200, 0.15, tri), 0.003, 0.06), peak=0.4)

# ---- Flow
write("countdown.wav", shaped(tone(880, 880, 0.15, tri), 0.002, 0.08), peak=0.6)
write("go.wav", shaped(tone(1320, 1320, 0.4, tri), 0.002, 0.2), peak=0.7)
win = []
for k, f in enumerate((523, 659, 784, 1047)):
    win = mix(win, [0.0] * n_of(k * 0.1) + shaped(mix(tone(f, f, 0.9, square), tone(f, f, 0.9)), 0.003, 0.35 if k == 3 else 0.12, 0.4))
write("round_win.wav", lowpass(win, 5000), peak=0.8)
write("ui_click.wav", shaped(tone(1500, 1500, 0.02), 0.001, 0.01), peak=0.5)
write("ui_confirm.wav", mix(shaped(tone(990, 990, 0.08, tri), 0.002, 0.04), [0.0] * n_of(0.05) + shaped(tone(1320, 1320, 0.1, tri), 0.002, 0.05)), peak=0.5)
write("ui_back.wav", mix(shaped(tone(990, 990, 0.08, tri), 0.002, 0.04), [0.0] * n_of(0.05) + shaped(tone(740, 740, 0.1, tri), 0.002, 0.05)), peak=0.5)

# ---- Music: 132 bpm, 8 bars in D minor (Dm Bb C A), loops in about 14.5 s
bpm = 132
beat = 60.0 / bpm
bars = 8
total = n_of(bars * 4 * beat)
stems = {"bass": [0.0] * total, "drums": [0.0] * total, "lead": [0.0] * total}
chords = [(62, 65, 69), (58, 62, 65), (60, 64, 67), (57, 61, 64)]


def midi(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def note(track, start, length, freq, shape, vol, decay):
    s0, n = n_of(start), n_of(length)
    buf = stems[track]
    for i in range(n):
        if s0 + i >= total:
            break
        t = i / RATE
        buf[s0 + i] += shape(2 * math.pi * freq * t) * vol * env(i, 0.003, decay) * (1 - i / n) ** 0.3


for bar in range(bars):
    c = chords[(bar // 2) % 4]
    t0 = bar * 4 * beat
    for e in range(8):  # bouncy eighth-note bass, octave jumps
        note("bass", t0 + e * beat / 2, beat / 2 * 0.85, midi(c[0] - 24 + (12 if e % 2 else 0)), tri, 0.5, beat * 0.3)
    for b in range(4):  # kick on every beat, snare on 2 and 4, hats on eighths
        s0 = n_of(t0 + b * beat)
        for i in range(n_of(0.15)):
            if s0 + i < total:
                stems["drums"][s0 + i] += math.sin(2 * math.pi * (110 - 60 * min(1, i / n_of(0.08))) * i / RATE) * math.exp(-i / RATE / 0.05) * 0.7
        if b % 2:
            sn = highpass(noise(n_of(0.12)), 1500)
            for i, v in enumerate(sn):
                if s0 + i < total:
                    stems["drums"][s0 + i] += v * math.exp(-i / RATE / 0.04) * 0.35
    for e in range(8):
        s0 = n_of(t0 + e * beat / 2)
        h = highpass(noise(n_of(0.03)), 7000)
        for i, v in enumerate(h):
            if s0 + i < total:
                stems["drums"][s0 + i] += v * math.exp(-i / RATE / 0.01) * 0.12
    melody = [c[2] + 12, c[1] + 12, c[2] + 12, c[0] + 24, c[2] + 12, c[1] + 12, c[0] + 12, c[1] + 12]
    for e, m in enumerate(melody):  # a sneaky staccato lead
        if (bar + e) % 3 == 2:
            continue
        note("lead", t0 + e * beat / 2, beat / 2 * 0.5, midi(m), square, 0.12, beat * 0.15)
fade = n_of(0.01)
for buf in stems.values():
    for i in range(fade):
        a = i / fade
        buf[total - fade + i] = buf[total - fade + i] * (1 - a) + buf[i] * a
mixed = [a + b + c for a, b, c in zip(stems["bass"], stems["drums"], stems["lead"])]
scale = 0.8 / max(max(abs(v) for v in mixed), 1e-9)
for name, buf in stems.items():
    f = lowpass(buf, 7000)
    write(f"music_{name}.wav", [v * scale for v in f], peak=max(abs(v) * scale for v in f))
