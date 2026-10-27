#include "gf.h"
#include "gf_platform.h"

#include <signal.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t s_quit;
static double s_t0;
static float s_fps;

static void on_signal(int sig)
{
    (void)sig;
    s_quit = 1;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

double gf_time(void)  { return now_s() - s_t0; }
float  gf_fps(void)   { return s_fps; }
void   gf_quit(void)  { s_quit = 1; }
int    gf_screen_width(void)  { return gf_platform_width(); }
int    gf_screen_height(void) { return gf_platform_height(); }

int gf_run(const gf_game *cfg)
{
    gf_game g = *cfg;
    int ret = 0;
    bool have_input = false, have_audio = false, have_batch = false, user_init = false;

    if (g.fixed_hz <= 0) g.fixed_hz = 60;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    s_quit = 0;
    s_fps = 0;

    if (!gf_assets_init(g.asset_root)) return 1;
    if (!gf_platform_init(g.width, g.height, g.vsync, g.depth_bits)) return 1;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    have_input = gf_input_init();
    if (g.audio) have_audio = gf_audio_init();
    if (!(have_batch = gf_batch_init())) { ret = 1; goto out; }

    if (g.init) {
        if (!g.init(g.user)) { ret = 1; goto out; }
        user_init = true;
    }

    const float cr = (float)(g.clear_color & 0xff) / 255.0f;
    const float cg = (float)((g.clear_color >> 8) & 0xff) / 255.0f;
    const float cb = (float)((g.clear_color >> 16) & 0xff) / 255.0f;
    const float ca = (float)((g.clear_color >> 24) & 0xff) / 255.0f;
    glClearColor(cr, cg, cb, ca);

    const double step = 1.0 / (double)g.fixed_hz;
    double prev = now_s(), acc = 0.0, fps_t = prev;
    int fps_frames = 0;
    s_t0 = prev;

    while (!s_quit) {
        double t = now_s();
        double frame = t - prev;
        prev = t;
        if (frame > 0.25) frame = 0.25;   /* debugger / SD-card stall: don't spiral */
        acc += frame;

        gf_input_poll();

        int steps = 0;
        while (acc >= step && steps < 5) {
            if (g.update) g.update((float)step, g.user);
            gf_input_end_update();
            acc -= step;
            steps++;
        }
        if (acc >= step) acc = 0.0;       /* too far behind: drop the backlog */

        glClear(GL_COLOR_BUFFER_BIT);
        if (g.draw) g.draw((float)(acc / step), g.user);
        gf_platform_swap();

        fps_frames++;
        if (t - fps_t >= 0.5) {
            s_fps = (float)(fps_frames / (t - fps_t));
            fps_frames = 0;
            fps_t = t;
        }
    }

out:
    if (user_init && g.shutdown) g.shutdown(g.user);
    if (have_batch) gf_batch_shutdown();
    if (have_audio) gf_audio_shutdown();
    if (have_input) gf_input_shutdown();
    gf_platform_shutdown();
    return ret;
}
