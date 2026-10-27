#ifndef GF_BATCH_H
#define GF_BATCH_H

#include <stdbool.h>
#include "gf_math.h"
#include "gf_texture.h"

/*
 * 2D sprite batcher. Quads are accumulated on the CPU and drawn with one
 * glDrawElements per texture change (max GF_BATCH_MAX_SPRITES per draw call).
 * Default projection is pixel space: (0,0) top-left, y down.
 *
 * gf_sprite is zero-init friendly:
 *   tex == NULL         -> solid colour quad (1x1 white texture)
 *   src.w/src.h == 0    -> whole texture
 *   size == (0,0)       -> source size in pixels
 *   color == 0          -> white
 *   origin              -> pivot as a fraction of size ((0.5,0.5) = centre)
 */
#define GF_BATCH_MAX_SPRITES 2048

typedef struct {
    const gf_texture *tex;
    gf_rect  src;
    gf_vec2  pos;
    gf_vec2  size;
    gf_vec2  origin;
    float    rotation;   /* radians, clockwise on screen (y down) */
    uint32_t color;
    bool     flip_x, flip_y;
} gf_sprite;

bool gf_batch_init(void);
void gf_batch_shutdown(void);

/* proj may be NULL for the default pixel-space projection. */
void gf_batch_begin(const gf_mat4 *proj);
void gf_batch_end(void);
void gf_batch_flush(void);

void gf_batch_sprite(const gf_sprite *s);
void gf_batch_draw(const gf_texture *t, float x, float y);   /* whole texture, 1:1 */
void gf_batch_rect(float x, float y, float w, float h, uint32_t color);

#endif
