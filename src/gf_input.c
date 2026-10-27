#include "gf.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_DEV 32
#define ABS_BYTES (ABS_MAX / 8 + 1)
#define BITS_BYTES(n) ((n) / 8 + 1)
#define TEST_BIT(bit, arr) ((arr)[(bit) / 8] & (1 << ((bit) % 8)))

typedef struct {
    int fd;
    struct input_absinfo abs[ABS_CNT];
} device;

static struct {
    device dev[MAX_DEV];
    uint8_t down[KEY_CNT], pressed[KEY_CNT], released[KEY_CNT];
    float axis[ABS_CNT];
    float mdx, mdy, wheel, mx, my;
    double last_scan;
    struct termios old_tio;
    bool tio_saved;
} I;

static double mono_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static float normalize(const struct input_absinfo *a, int value)
{
    int range = a->maximum - a->minimum;
    if (range <= 0) return 0.0f;
    return ((float)(value - a->minimum) / (float)range) * 2.0f - 1.0f;
}

static void open_device(int idx)
{
    char path[64];
    unsigned char evbits[BITS_BYTES(EV_MAX)];
    device *d = &I.dev[idx];

    snprintf(path, sizeof path, "/dev/input/event%d", idx);
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return;

    memset(evbits, 0, sizeof evbits);
    if (ioctl(fd, EVIOCGBIT(0, sizeof evbits), evbits) < 0 ||
        !(TEST_BIT(EV_KEY, evbits) || TEST_BIT(EV_REL, evbits) || TEST_BIT(EV_ABS, evbits))) {
        close(fd);
        return;
    }

    memset(d, 0, sizeof *d);
    d->fd = fd;

    if (TEST_BIT(EV_ABS, evbits)) {
        unsigned char absbits[ABS_BYTES];
        memset(absbits, 0, sizeof absbits);
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof absbits), absbits);
        for (int c = 0; c < ABS_CNT; c++) {
            if (!TEST_BIT(c, absbits)) continue;
            if (ioctl(fd, EVIOCGABS(c), &d->abs[c]) == 0)
                I.axis[c] = normalize(&d->abs[c], d->abs[c].value);
        }
    }

    char name[128] = "?";
    ioctl(fd, EVIOCGNAME(sizeof name), name);
    GF_LOGI("input: %s -> %s", path, name);
}

static void close_device(int idx)
{
    if (I.dev[idx].fd > 0) close(I.dev[idx].fd);
    I.dev[idx].fd = -1;
    memset(I.down, 0, sizeof I.down);   /* avoid keys stuck down after unplug */
}

static void rescan(void)
{
    for (int i = 0; i < MAX_DEV; i++)
        if (I.dev[i].fd <= 0) open_device(i);
}

static void handle_event(device *d, const struct input_event *e)
{
    switch (e->type) {
    case EV_KEY:
        if (e->code >= KEY_CNT) break;
        if (e->value == 1) {
            I.down[e->code] = 1;
            I.pressed[e->code] = 1;
        } else if (e->value == 0) {
            I.down[e->code] = 0;
            I.released[e->code] = 1;
        }
        break;
    case EV_REL:
        if (e->code == REL_X) {
            I.mdx += (float)e->value;
            I.mx = gf_clampf(I.mx + (float)e->value, 0.0f, (float)gf_screen_width() - 1.0f);
        } else if (e->code == REL_Y) {
            I.mdy += (float)e->value;
            I.my = gf_clampf(I.my + (float)e->value, 0.0f, (float)gf_screen_height() - 1.0f);
        } else if (e->code == REL_WHEEL) {
            I.wheel += (float)e->value;
        }
        break;
    case EV_ABS:
        if (e->code < ABS_CNT) I.axis[e->code] = normalize(&d->abs[e->code], e->value);
        break;
    default:
        break;
    }
}

bool gf_input_init(void)
{
    memset(&I, 0, sizeof I);
    for (int i = 0; i < MAX_DEV; i++) I.dev[i].fd = -1;

    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &I.old_tio) == 0) {
        struct termios t = I.old_tio;
        t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
        I.tio_saved = true;
        if (write(STDOUT_FILENO, "\033[?25l", 6) < 0) { /* ignore */ }
    }

    I.mx = (float)gf_screen_width() * 0.5f;
    I.my = (float)gf_screen_height() * 0.5f;
    I.last_scan = mono_s();
    rescan();
    return true;
}

void gf_input_shutdown(void)
{
    for (int i = 0; i < MAX_DEV; i++)
        if (I.dev[i].fd > 0) close(I.dev[i].fd);
    if (I.tio_saved) {
        tcflush(STDIN_FILENO, TCIFLUSH);
        tcsetattr(STDIN_FILENO, TCSANOW, &I.old_tio);
        if (write(STDOUT_FILENO, "\033[?25h", 6) < 0) { /* ignore */ }
    }
}

void gf_input_poll(void)
{
    double t = mono_s();
    if (t - I.last_scan > 2.0) {
        I.last_scan = t;
        rescan();
    }

    for (int i = 0; i < MAX_DEV; i++) {
        device *d = &I.dev[i];
        if (d->fd <= 0) continue;
        for (;;) {
            struct input_event ev[64];
            ssize_t n = read(d->fd, ev, sizeof ev);
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno != EAGAIN) close_device(i);   /* ENODEV: unplugged */
                break;
            }
            if (n == 0) break;
            for (size_t k = 0; k < (size_t)n / sizeof ev[0]; k++) handle_event(d, &ev[k]);
        }
    }

    /* Keep keystrokes from piling up in the tty and landing in the shell. */
    if (I.tio_saved) tcflush(STDIN_FILENO, TCIFLUSH);
}

void gf_input_end_update(void)
{
    memset(I.pressed, 0, sizeof I.pressed);
    memset(I.released, 0, sizeof I.released);
    I.mdx = I.mdy = I.wheel = 0.0f;
}

bool gf_key_down(int c)     { return c >= 0 && c < KEY_CNT && I.down[c]; }
bool gf_key_pressed(int c)  { return c >= 0 && c < KEY_CNT && I.pressed[c]; }
bool gf_key_released(int c) { return c >= 0 && c < KEY_CNT && I.released[c]; }

float gf_axis(int c, float dz)
{
    if (c < 0 || c >= ABS_CNT) return 0.0f;
    float v = I.axis[c];
    if (fabsf(v) < dz) return 0.0f;
    return v;
}

float gf_mouse_dx(void)    { return I.mdx; }
float gf_mouse_dy(void)    { return I.mdy; }
float gf_mouse_wheel(void) { return I.wheel; }
float gf_mouse_x(void)     { return I.mx; }
float gf_mouse_y(void)     { return I.my; }
