#ifndef GF_CORE_H
#define GF_CORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Game description passed to gf_run(). All fields are optional (zero = default).
 * The loop is fixed-timestep: update() runs fixed_hz times per second with a
 * constant dt, draw() runs once per displayed frame with alpha in [0,1) giving
 * the position between the last two updates (for interpolation, optional).
 */
typedef struct gf_game {
    int  width, height;      /* render size; 0 = native display size.
                                Smaller than the display => GPU upscales. */
    int  vsync;              /* eglSwapInterval; 0 = off (zero-init default), 1 = sync to display */
    int  fixed_hz;           /* update rate, default 60 */
    int  depth_bits;         /* 0 for pure 2D (default), 16 for 3D */
    bool audio;              /* start the ALSA mixer */
    const char *asset_root;  /* NULL = auto ("assets") */
    uint32_t clear_color;    /* GF_RGBA(...) cleared before draw() */

    bool (*init)(void *user);            /* GL is ready; return false to abort */
    void (*update)(float dt, void *user);
    void (*draw)(float alpha, void *user);
    void (*shutdown)(void *user);        /* called while GL is still valid */
    void *user;
} gf_game;

/* Runs until gf_quit(), SIGINT or SIGTERM. Returns 0 on clean exit. */
int    gf_run(const gf_game *game);
void   gf_quit(void);

int    gf_screen_width(void);
int    gf_screen_height(void);
double gf_time(void);   /* seconds since gf_run started */
float  gf_fps(void);

#endif
