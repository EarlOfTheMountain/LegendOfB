#ifndef GF_PLATFORM_H
#define GF_PLATFORM_H

#include <stdbool.h>

/*
 * Window/GL context on bare VideoCore (no X): dispmanx element + EGL surface.
 * If w/h are smaller than the display, the GPU scales the surface up
 * (aspect preserved, letterboxed) for free. 0 means native display size.
 */
bool gf_platform_init(int w, int h, int vsync, int depth_bits);
void gf_platform_shutdown(void);
void gf_platform_swap(void);
int  gf_platform_width(void);
int  gf_platform_height(void);

#endif
