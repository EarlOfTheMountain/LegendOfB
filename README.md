# gf – a small GLES2 game framework for the Raspberry Pi (no X)

Targets Raspbian Buster on a Pi 1 Model B: dispmanx + EGL + OpenGL ES 2.0,
evdev input, ALSA audio. Plain C (gnu11).

```
include/   public headers (include "gf.h")
src/       framework sources + main.c (the demo game; replace it)
assets/    shaders/  images/  sounds/
tools/     make_assets.py regenerates the demo assets
```

## Setup (on the Pi)

```
sudo apt install build-essential libasound2-dev
sudo adduser $USER input && sudo adduser $USER audio && sudo adduser $USER video   # then log in again
make run
```

Make sure `gpu_mem` in `/boot/config.txt` is at least 64 (textures live in GPU memory)
and that you are not using the experimental `vc4-kms-v3d` overlay (the Broadcom
`libbrcmEGL` stack used here is the legacy driver).

## Modules

| Header | What it does |
|---|---|
| `gf_core.h` | `gf_run()` fixed-timestep loop, signal handling, clean shutdown |
| `gf_platform.h` | dispmanx element + EGL context; render size smaller than the display is upscaled by the GPU |
| `gf_assets.h` | asset root lookup, `gf_asset_read()` |
| `gf_shader.h` | compile/link from `assets/shaders/*`, uniform lookup |
| `gf_texture.h` | TGA loading, RGBA upload |
| `gf_batch.h` | sprite batcher (2048 quads/draw call, pixel-space projection by default) |
| `gf_input.h` | keyboard / mouse / gamepad through `/dev/input/event*`, hot-plug |
| `gf_audio.h` | WAV loading, 16-voice mixer thread, ALSA output |
| `gf_math.h` | vec2, rect, mat4 helpers, `GF_RGBA` |

## Pi 1 performance notes

- Render at 640x480 or 800x600 and let the GPU scale (`gf_game.width/height`); 1080p fill rate is brutal on a Pi 1.
- Draw sprites from as few textures as possible (atlas them); every texture change flushes the batch.
- Sounds are expanded to 44.1 kHz stereo 16-bit in RAM (~176 KB per second), whatever the file's format, so keep clips short; there is no music streaming yet.
- If the console text shows through or the terminal misbehaves after a crash, run `reset`.
