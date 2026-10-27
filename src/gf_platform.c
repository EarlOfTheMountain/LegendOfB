#include "gf_platform.h"
#include "gf_gl.h"
#include "gf_log.h"

#include <bcm_host.h>
#include <string.h>

static struct {
    bool inited;
    EGLDisplay dpy;
    EGLSurface surf;
    EGLContext ctx;
    DISPMANX_DISPLAY_HANDLE_T dm_display;
    DISPMANX_ELEMENT_HANDLE_T dm_element;
    EGL_DISPMANX_WINDOW_T win;
    int w, h;
} P;

void gf_gl_check_(const char *where, const char *file, int line)
{
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR)
        gf_log_impl(GF_LOG_ERROR, file, line, "GL error 0x%04x at %s", (unsigned)e, where);
}

bool gf_platform_init(int req_w, int req_h, int vsync, int depth_bits)
{
    uint32_t dw = 0, dh = 0;

    memset(&P, 0, sizeof P);
    P.dpy = EGL_NO_DISPLAY;
    P.surf = EGL_NO_SURFACE;
    P.ctx = EGL_NO_CONTEXT;

    bcm_host_init();
    P.inited = true;

    if (graphics_get_display_size(0, &dw, &dh) < 0 || dw == 0 || dh == 0) {
        GF_LOGE("graphics_get_display_size failed");
        goto fail;
    }

    int w = req_w > 0 ? req_w : (int)dw;
    int h = req_h > 0 ? req_h : (int)dh;

    /* Fit the surface into the display, keeping aspect; the VideoCore
     * compositor does the scaling in hardware. */
    uint32_t fw = dw;
    uint32_t fh = (uint32_t)(((uint64_t)dw * (uint32_t)h) / (uint32_t)w);
    if (fh > dh) {
        fh = dh;
        fw = (uint32_t)(((uint64_t)dh * (uint32_t)w) / (uint32_t)h);
    }
    fw &= ~1u;
    fh &= ~1u;

    VC_RECT_T dst, src;
    vc_dispmanx_rect_set(&dst, (dw - fw) / 2, (dh - fh) / 2, fw, fh);
    vc_dispmanx_rect_set(&src, 0, 0, (uint32_t)w << 16, (uint32_t)h << 16);

    VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 255, 0 };

    P.dm_display = vc_dispmanx_display_open(0);
    DISPMANX_UPDATE_HANDLE_T up = vc_dispmanx_update_start(0);
    P.dm_element = vc_dispmanx_element_add(up, P.dm_display, 0 /*layer*/, &dst, 0,
                                           &src, DISPMANX_PROTECTION_NONE, &alpha,
                                           NULL, DISPMANX_NO_ROTATE);
    P.win.element = P.dm_element;
    P.win.width = w;
    P.win.height = h;
    vc_dispmanx_update_submit_sync(up);

    P.dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (P.dpy == EGL_NO_DISPLAY || !eglInitialize(P.dpy, NULL, NULL)) {
        GF_LOGE("eglInitialize failed (0x%x)", (unsigned)eglGetError());
        goto fail;
    }
    eglBindAPI(EGL_OPENGL_ES_API);

    const EGLint cfg_attr[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, depth_bits,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint ncfg = 0;
    if (!eglChooseConfig(P.dpy, cfg_attr, &cfg, 1, &ncfg) || ncfg < 1) {
        GF_LOGE("no matching EGL config (0x%x)", (unsigned)eglGetError());
        goto fail;
    }

    const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    P.ctx = eglCreateContext(P.dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (P.ctx == EGL_NO_CONTEXT) {
        GF_LOGE("eglCreateContext failed (0x%x)", (unsigned)eglGetError());
        goto fail;
    }

    P.surf = eglCreateWindowSurface(P.dpy, cfg, (EGLNativeWindowType)&P.win, NULL);
    if (P.surf == EGL_NO_SURFACE) {
        GF_LOGE("eglCreateWindowSurface failed (0x%x)", (unsigned)eglGetError());
        goto fail;
    }

    if (!eglMakeCurrent(P.dpy, P.surf, P.surf, P.ctx)) {
        GF_LOGE("eglMakeCurrent failed (0x%x)", (unsigned)eglGetError());
        goto fail;
    }
    eglSwapInterval(P.dpy, vsync);

    P.w = w;
    P.h = h;
    glViewport(0, 0, w, h);

    GF_LOGI("display %ux%u, surface %dx%d -> %ux%u, %s", dw, dh, w, h, fw, fh,
            (const char *)glGetString(GL_RENDERER));
    return true;

fail:
    gf_platform_shutdown();
    return false;
}

void gf_platform_shutdown(void)
{
    if (!P.inited) return;

    if (P.dpy != EGL_NO_DISPLAY) {
        if (P.surf != EGL_NO_SURFACE && P.ctx != EGL_NO_CONTEXT) {
            /* leave a black screen behind instead of the last frame */
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            eglSwapBuffers(P.dpy, P.surf);
        }
        eglMakeCurrent(P.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (P.surf != EGL_NO_SURFACE) eglDestroySurface(P.dpy, P.surf);
        if (P.ctx != EGL_NO_CONTEXT) eglDestroyContext(P.dpy, P.ctx);
        eglTerminate(P.dpy);
    }

    if (P.dm_element) {
        DISPMANX_UPDATE_HANDLE_T up = vc_dispmanx_update_start(0);
        vc_dispmanx_element_remove(up, P.dm_element);
        vc_dispmanx_update_submit_sync(up);
    }
    if (P.dm_display) vc_dispmanx_display_close(P.dm_display);

    bcm_host_deinit();
    memset(&P, 0, sizeof P);
}

void gf_platform_swap(void) { eglSwapBuffers(P.dpy, P.surf); }
int  gf_platform_width(void) { return P.w; }
int  gf_platform_height(void) { return P.h; }
