#ifndef GF_TEXTURE_H
#define GF_TEXTURE_H

#include <stdbool.h>
#include "gf_gl.h"

typedef enum { GF_FILTER_NEAREST, GF_FILTER_LINEAR } gf_filter;

typedef struct {
    GLuint id;
    int w, h;
} gf_texture;

/* Loads assets/<rel>. Supported: TGA (24/32-bit truecolor and 8-bit gray,
 * raw or RLE). Convert others first, e.g. `convert in.png out.tga`.
 * Textures use clamp-to-edge and no mipmaps (works for non-power-of-two on
 * VideoCore IV). Row 0 of the image is v=0 (top-left origin). */
bool gf_texture_load(gf_texture *t, const char *rel, gf_filter filter);
bool gf_texture_from_rgba(gf_texture *t, int w, int h, const void *rgba, gf_filter filter);
void gf_texture_bind(const gf_texture *t, int unit);
void gf_texture_free(gf_texture *t);

#endif
