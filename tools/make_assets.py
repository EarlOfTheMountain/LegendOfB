#!/usr/bin/env python3
"""Generates the demo assets: assets/images/ball.tga and assets/sounds/beep.wav."""
import math
import os
import struct
import wave

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")


def make_ball(path, size=64):
    """White shaded ball with a soft edge, 32-bit uncompressed TGA, top-left origin."""
    px = bytearray()
    c = (size - 1) / 2.0
    r = size / 2.0 - 1.5
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - c, y - c)
            a = max(0.0, min(1.0, r - d + 1.0))
            # simple highlight so tinting looks 3D
            hx, hy = (x - c * 0.7) / size, (y - c * 0.7) / size
            shade = 1.0 - min(1.0, math.hypot(hx, hy)) * 0.45
            v = int(255 * shade)
            px += bytes((v, v, v, int(255 * a)))   # B, G, R, A
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 0x28)
    with open(path, "wb") as f:
        f.write(header + bytes(px))


def make_beep(path, rate=22050, secs=0.25, freq=660.0):
    n = int(rate * secs)
    frames = bytearray()
    for i in range(n):
        env = math.exp(-6.0 * i / n)
        s = int(32767 * 0.6 * env * math.sin(2 * math.pi * freq * i / rate))
        frames += struct.pack("<h", s)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "images"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "sounds"), exist_ok=True)
    make_ball(os.path.join(ROOT, "images", "ball.tga"))
    make_beep(os.path.join(ROOT, "sounds", "beep.wav"))
    print("assets written to", os.path.normpath(ROOT))
