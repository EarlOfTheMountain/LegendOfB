#include "gf_texture.h"
#include "gf_assets.h"
#include "gf_log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Decodes TGA types 2/3 (raw) and 10/11 (RLE): 24/32-bit colour or 8-bit gray.
 * Returns malloc'd RGBA8, top row first. */
static uint8_t *tga_decode(const uint8_t *d, size_t n, int *ow, int *oh)
{
    if (n < 18) return NULL;

    const int idlen = d[0], cmap = d[1], type = d[2];
    const int w = d[12] | (d[13] << 8), h = d[14] | (d[15] << 8);
    const int bpp = d[16], desc = d[17];
    const int rle = (type == 10 || type == 11);
    const int gray = (type == 3 || type == 11);

    if (cmap != 0 || (type != 2 && type != 3 && type != 10 && type != 11)) return NULL;
    if (w <= 0 || h <= 0) return NULL;
    if (gray ? bpp != 8 : (bpp != 24 && bpp != 32)) return NULL;

    const int bytes = bpp / 8;
    const uint8_t *p = d + 18 + idlen;
    const uint8_t *end = d + n;
    const size_t total = (size_t)w * (size_t)h;

    uint8_t *out = malloc(total * 4);
    if (!out) return NULL;

    uint8_t px[4] = { 0, 0, 0, 255 };
    size_t i = 0;
    while (i < total) {
        size_t run = 1;
        int raw = 1;
        if (rle) {
            if (p >= end) goto bad;
            uint8_t hdr = *p++;
            run = (size_t)(hdr & 127) + 1;
            raw = !(hdr & 128);
        }
        for (size_t k = 0; k < run && i < total; k++) {
            if (raw || k == 0) {
                if (p + bytes > end) goto bad;
                if (gray) {
                    px[0] = px[1] = px[2] = p[0];
                    px[3] = 255;
                } else {
                    px[0] = p[2];
                    px[1] = p[1];
                    px[2] = p[0];
                    px[3] = (bytes == 4) ? p[3] : 255;
                }
                p += bytes;
            }
            memcpy(out + i * 4, px, 4);
            i++;
        }
    }

    /* bit 5 of the descriptor set = origin at top-left (already top first) */
    if (!(desc & 0x20)) {
        const size_t row = (size_t)w * 4;
        uint8_t *tmp = malloc(row);
        if (!tmp) goto bad;
        for (int y = 0; y < h / 2; y++) {
            uint8_t *a = out + (size_t)y * row;
            uint8_t *b = out + (size_t)(h - 1 - y) * row;
            memcpy(tmp, a, row);
            memcpy(a, b, row);
            memcpy(b, tmp, row);
        }
        free(tmp);
    }

    *ow = w;
    *oh = h;
    return out;

bad:
    free(out);
    return NULL;
}

bool gf_texture_from_rgba(gf_texture *t, int w, int h, const void *rgba, gf_filter filter)
{
    const GLint f = (filter == GF_FILTER_NEAREST) ? GL_NEAREST : GL_LINEAR;

    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    t->w = w;
    t->h = h;

    if (glGetError() != GL_NO_ERROR) {
        GF_LOGE("texture upload failed (%dx%d)", w, h);
        glDeleteTextures(1, &t->id);
        t->id = 0;
        return false;
    }
    return true;
}

bool gf_texture_load(gf_texture *t, const char *rel, gf_filter filter)
{
    memset(t, 0, sizeof *t);

    size_t len = strlen(rel);
    if (len < 4 || strcmp(rel + len - 4, ".tga") != 0) {
        GF_LOGE("'%s': only .tga images are supported (convert with `convert in.png out.tga`)", rel);
        return false;
    }

    size_t n = 0;
    uint8_t *file = gf_asset_read(rel, &n);
    if (!file) return false;

    int w = 0, h = 0;
    uint8_t *px = tga_decode(file, n, &w, &h);
    free(file);
    if (!px) {
        GF_LOGE("'%s': unsupported or corrupt TGA", rel);
        return false;
    }

    bool ok = gf_texture_from_rgba(t, w, h, px, filter);
    free(px);
    return ok;
}

void gf_texture_bind(const gf_texture *t, int unit)
{
    glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
    glBindTexture(GL_TEXTURE_2D, t->id);
}

void gf_texture_free(gf_texture *t)
{
    if (t->id) glDeleteTextures(1, &t->id);
    memset(t, 0, sizeof *t);
}
