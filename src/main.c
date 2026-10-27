/*
 * Demo game: a swarm of bouncing, spinning sprites plus a player square.
 *   Arrow keys / WASD / gamepad left stick : move the square
 *   Space / gamepad A (BTN_SOUTH)          : beep
 *   Esc / Ctrl-C                           : quit
 * Replace this file with your own game; everything else is the framework.
 */
#include "gf.h"

#include <stdio.h>
#include <stdlib.h>

#define NUM_BALLS 400

typedef struct {
    float x, y, vx, vy, spin, angle;
    uint32_t color;
} ball;

static struct {
    gf_texture tex;
    gf_sound  *beep;
    ball       balls[NUM_BALLS];
    float      px, py;
} G;

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

static bool game_init(void *user)
{
    (void)user;
    if (!gf_texture_load(&G.tex, "images/ball.tga", GF_FILTER_LINEAR)) return false;
    G.beep = gf_sound_load("sounds/beep.wav");   /* may be NULL: game runs silent */

    const float sw = (float)gf_screen_width(), sh = (float)gf_screen_height();
    for (int i = 0; i < NUM_BALLS; i++) {
        ball *b = &G.balls[i];
        b->x = frand(32, sw - 32);
        b->y = frand(32, sh - 32);
        b->vx = frand(-120, 120);
        b->vy = frand(-120, 120);
        b->spin = frand(-3, 3);
        b->color = GF_RGBA((int)frand(80, 255), (int)frand(80, 255), (int)frand(80, 255), 255);
    }
    G.px = sw * 0.5f;
    G.py = sh * 0.5f;
    return true;
}

static void game_update(float dt, void *user)
{
    (void)user;
    const float sw = (float)gf_screen_width(), sh = (float)gf_screen_height();

    if (gf_key_pressed(KEY_ESC)) gf_quit();
    if (gf_key_pressed(KEY_SPACE) || gf_key_pressed(BTN_SOUTH))
        gf_sound_play(G.beep, 0.6f, false);

    float dx = gf_axis(ABS_X, 0.15f), dy = gf_axis(ABS_Y, 0.15f);
    if (gf_key_down(KEY_LEFT)  || gf_key_down(KEY_A)) dx -= 1.0f;
    if (gf_key_down(KEY_RIGHT) || gf_key_down(KEY_D)) dx += 1.0f;
    if (gf_key_down(KEY_UP)    || gf_key_down(KEY_W)) dy -= 1.0f;
    if (gf_key_down(KEY_DOWN)  || gf_key_down(KEY_S)) dy += 1.0f;
    G.px = gf_clampf(G.px + dx * 300.0f * dt, 16.0f, sw - 16.0f);
    G.py = gf_clampf(G.py + dy * 300.0f * dt, 16.0f, sh - 16.0f);

    for (int i = 0; i < NUM_BALLS; i++) {
        ball *b = &G.balls[i];
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        b->angle += b->spin * dt;
        if (b->x < 16 || b->x > sw - 16) { b->vx = -b->vx; b->x = gf_clampf(b->x, 16, sw - 16); }
        if (b->y < 16 || b->y > sh - 16) { b->vy = -b->vy; b->y = gf_clampf(b->y, 16, sh - 16); }
    }
}

static void game_draw(float alpha, void *user)
{
    (void)alpha;
    (void)user;

    gf_batch_begin(NULL);
    for (int i = 0; i < NUM_BALLS; i++) {
        const ball *b = &G.balls[i];
        gf_sprite s = { 0 };
        s.tex = &G.tex;
        s.pos = gf_v2(b->x, b->y);
        s.size = gf_v2(32, 32);
        s.origin = gf_v2(0.5f, 0.5f);
        s.rotation = b->angle;
        s.color = b->color;
        gf_batch_sprite(&s);
    }
    gf_batch_rect(G.px - 16, G.py - 16, 32, 32, GF_RGBA(255, 255, 255, 255));
    gf_batch_end();

    static float last = 0;
    if (gf_time() - last > 1.0) {
        last = (float)gf_time();
        printf("\r%.1f fps  ", (double)gf_fps());
        fflush(stdout);
    }
}

static void game_shutdown(void *user)
{
    (void)user;
    gf_sound_free(G.beep);
    gf_texture_free(&G.tex);
}

int main(void)
{
    gf_game g = { 0 };
    g.width = 640;             /* render small, the GPU scales to the display */
    g.height = 480;
    g.vsync = 1;
    g.fixed_hz = 60;
    g.audio = true;
    g.clear_color = GF_RGBA(18, 22, 38, 255);
    g.init = game_init;
    g.update = game_update;
    g.draw = game_draw;
    g.shutdown = game_shutdown;
    return gf_run(&g);
}
