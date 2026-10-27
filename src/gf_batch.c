#include "gf_batch.h"
#include "gf_gl.h"
#include "gf_log.h"
#include "gf_shader.h"
#include "gf_core.h"

#include <math.h>
#include <stddef.h>

typedef struct {
    float x, y, u, v;
    uint32_t c;
} vertex;

static struct {
    GLuint prog, vbo, ibo;
    GLint  u_proj, u_tex;
    GLuint cur_tex;
    int    count;               /* sprites queued */
    bool   active;
    gf_texture white;
} B;

static vertex   s_verts[GF_BATCH_MAX_SPRITES * 4];
static uint16_t s_indices[GF_BATCH_MAX_SPRITES * 6];

bool gf_batch_init(void)
{
    static const char *const attribs[] = { "a_pos", "a_uv", "a_color", NULL };
    const uint8_t white_px[4] = { 255, 255, 255, 255 };

    B.prog = gf_shader_load("shaders/sprite.vert", "shaders/sprite.frag", attribs);
    if (!B.prog) return false;
    B.u_proj = gf_uniform(B.prog, "u_proj");
    B.u_tex  = gf_uniform(B.prog, "u_tex");

    for (int i = 0; i < GF_BATCH_MAX_SPRITES; i++) {
        uint16_t b = (uint16_t)(i * 4);
        uint16_t *q = &s_indices[i * 6];
        q[0] = b;     q[1] = (uint16_t)(b + 1); q[2] = (uint16_t)(b + 2);
        q[3] = (uint16_t)(b + 1); q[4] = (uint16_t)(b + 3); q[5] = (uint16_t)(b + 2);
    }

    glGenBuffers(1, &B.vbo);
    glGenBuffers(1, &B.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, B.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof s_indices, s_indices, GL_STATIC_DRAW);

    if (!gf_texture_from_rgba(&B.white, 1, 1, white_px, GF_FILTER_NEAREST)) return false;

    B.count = 0;
    B.active = false;
    return true;
}

void gf_batch_shutdown(void)
{
    gf_texture_free(&B.white);
    if (B.vbo) glDeleteBuffers(1, &B.vbo);
    if (B.ibo) glDeleteBuffers(1, &B.ibo);
    gf_shader_free(B.prog);
    B.vbo = B.ibo = B.prog = 0;
}

void gf_batch_begin(const gf_mat4 *proj)
{
    gf_mat4 p;
    if (proj) p = *proj;
    else p = gf_mat4_ortho(0.0f, (float)gf_screen_width(), (float)gf_screen_height(), 0.0f, -1.0f, 1.0f);

    glUseProgram(B.prog);
    glUniformMatrix4fv(B.u_proj, 1, GL_FALSE, p.m);
    glUniform1i(B.u_tex, 0);
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    B.count = 0;
    B.cur_tex = 0;
    B.active = true;
}

void gf_batch_flush(void)
{
    if (B.count == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, B.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)B.count * 4 * sizeof(vertex)),
                 s_verts, GL_STREAM_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void *)offsetof(vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const void *)offsetof(vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (const void *)offsetof(vertex, c));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, B.ibo);
    glBindTexture(GL_TEXTURE_2D, B.cur_tex);
    glDrawElements(GL_TRIANGLES, B.count * 6, GL_UNSIGNED_SHORT, (const void *)0);

    B.count = 0;
}

void gf_batch_end(void)
{
    gf_batch_flush();
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    B.active = false;
}

void gf_batch_sprite(const gf_sprite *s)
{
    const gf_texture *t = s->tex ? s->tex : &B.white;

    if (B.count > 0 && (t->id != B.cur_tex || B.count >= GF_BATCH_MAX_SPRITES))
        gf_batch_flush();
    B.cur_tex = t->id;

    float sx = s->src.x, sy = s->src.y, sw = s->src.w, sh = s->src.h;
    if (sw == 0.0f || sh == 0.0f) { sx = 0; sy = 0; sw = (float)t->w; sh = (float)t->h; }

    float w = s->size.x, h = s->size.y;
    if (w == 0.0f && h == 0.0f) { w = fabsf(sw); h = fabsf(sh); }

    float u0 = sx / (float)t->w,        v0 = sy / (float)t->h;
    float u1 = (sx + sw) / (float)t->w, v1 = (sy + sh) / (float)t->h;
    if (s->flip_x) { float tmp = u0; u0 = u1; u1 = tmp; }
    if (s->flip_y) { float tmp = v0; v0 = v1; v1 = tmp; }

    const float ox = s->origin.x * w, oy = s->origin.y * h;
    const float lx[4] = { -ox, w - ox, -ox,    w - ox };
    const float ly[4] = { -oy, -oy,    h - oy, h - oy };
    const float us[4] = { u0, u1, u0, u1 };
    const float vs[4] = { v0, v0, v1, v1 };
    const uint32_t col = s->color ? s->color : GF_WHITE;

    float c = 1.0f, sn = 0.0f;
    if (s->rotation != 0.0f) { c = cosf(s->rotation); sn = sinf(s->rotation); }

    vertex *v = &s_verts[B.count * 4];
    for (int i = 0; i < 4; i++) {
        v[i].x = s->pos.x + lx[i] * c - ly[i] * sn;
        v[i].y = s->pos.y + lx[i] * sn + ly[i] * c;
        v[i].u = us[i];
        v[i].v = vs[i];
        v[i].c = col;
    }
    B.count++;
}

void gf_batch_draw(const gf_texture *t, float x, float y)
{
    gf_sprite s = { 0 };
    s.tex = t;
    s.pos = gf_v2(x, y);
    gf_batch_sprite(&s);
}

void gf_batch_rect(float x, float y, float w, float h, uint32_t color)
{
    gf_sprite s = { 0 };
    s.pos = gf_v2(x, y);
    s.size = gf_v2(w, h);
    s.color = color;
    gf_batch_sprite(&s);
}
