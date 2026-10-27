#ifndef GF_GL_H
#define GF_GL_H

/* One place that pulls in EGL + GLES2 (Broadcom VideoCore IV, /opt/vc). */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

void gf_gl_check_(const char *where, const char *file, int line);

/* Compiled in only with -DGF_DEBUG; glGetError stalls the pipeline. */
#ifdef GF_DEBUG
#define GF_GLCHECK(where) gf_gl_check_((where), __FILE__, __LINE__)
#else
#define GF_GLCHECK(where) ((void)0)
#endif

#endif
