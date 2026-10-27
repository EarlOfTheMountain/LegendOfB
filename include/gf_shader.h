#ifndef GF_SHADER_H
#define GF_SHADER_H

#include "gf_gl.h"

/*
 * attribs: NULL-terminated array of attribute names; the name at index i is
 * bound to attribute location i before linking (may be NULL).
 * Returns the program id, or 0 on failure (compile/link log goes to stderr).
 */
GLuint gf_shader_build(const char *vert_src, const char *frag_src, const char *const *attribs);
GLuint gf_shader_load(const char *vert_rel, const char *frag_rel, const char *const *attribs);
void   gf_shader_free(GLuint program);

GLint  gf_uniform(GLuint program, const char *name);

#endif
