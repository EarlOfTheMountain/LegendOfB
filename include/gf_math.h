#ifndef GF_MATH_H
#define GF_MATH_H

#include <math.h>
#include <stdint.h>

#define GF_PI 3.14159265358979f

/* Packed RGBA, byte order R,G,B,A in memory (little-endian uint32). */
#define GF_RGBA(r, g, b, a) \
    ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))
#define GF_WHITE GF_RGBA(255, 255, 255, 255)

typedef struct { float x, y; } gf_vec2;
typedef struct { float x, y, w, h; } gf_rect;
/* Column-major, as OpenGL expects. */
typedef struct { float m[16]; } gf_mat4;

static inline gf_vec2 gf_v2(float x, float y) { gf_vec2 v = { x, y }; return v; }
static inline float gf_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float gf_lerpf(float a, float b, float t) { return a + (b - a) * t; }

static inline gf_mat4 gf_mat4_identity(void)
{
    gf_mat4 r = { { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 } };
    return r;
}

static inline gf_mat4 gf_mat4_ortho(float l, float r, float b, float t, float n, float f)
{
    gf_mat4 o = { { 0 } };
    o.m[0]  = 2.0f / (r - l);
    o.m[5]  = 2.0f / (t - b);
    o.m[10] = -2.0f / (f - n);
    o.m[12] = -(r + l) / (r - l);
    o.m[13] = -(t + b) / (t - b);
    o.m[14] = -(f + n) / (f - n);
    o.m[15] = 1.0f;
    return o;
}

static inline gf_mat4 gf_mat4_mul(gf_mat4 a, gf_mat4 b)
{
    gf_mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

static inline gf_mat4 gf_mat4_translate(float x, float y, float z)
{
    gf_mat4 r = gf_mat4_identity();
    r.m[12] = x; r.m[13] = y; r.m[14] = z;
    return r;
}

static inline gf_mat4 gf_mat4_scale(float x, float y, float z)
{
    gf_mat4 r = gf_mat4_identity();
    r.m[0] = x; r.m[5] = y; r.m[10] = z;
    return r;
}

static inline gf_mat4 gf_mat4_rotate_z(float rad)
{
    gf_mat4 r = gf_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
    return r;
}

#endif
