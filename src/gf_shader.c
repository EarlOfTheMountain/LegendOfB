#include "gf_shader.h"
#include "gf_assets.h"
#include "gf_log.h"

#include <stdlib.h>

static GLuint compile(GLenum type, const char *src, const char *what)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        GF_LOGE("%s shader failed to compile:\n%s", what, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint gf_shader_build(const char *vsrc, const char *fsrc, const char *const *attribs)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc, "vertex");
    if (!vs) return 0;
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, "fragment");
    if (!fs) { glDeleteShader(vs); return 0; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    if (attribs)
        for (GLuint i = 0; attribs[i]; i++)
            glBindAttribLocation(prog, i, attribs[i]);
    glLinkProgram(prog);

    /* flagged for deletion; they go away with the program */
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        GF_LOGE("shader link failed:\n%s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

GLuint gf_shader_load(const char *vert_rel, const char *frag_rel, const char *const *attribs)
{
    char *vs = gf_asset_read(vert_rel, NULL);
    char *fs = gf_asset_read(frag_rel, NULL);
    GLuint prog = 0;
    if (vs && fs) prog = gf_shader_build(vs, fs, attribs);
    free(vs);
    free(fs);
    return prog;
}

void gf_shader_free(GLuint program)
{
    if (program) glDeleteProgram(program);
}

GLint gf_uniform(GLuint program, const char *name)
{
    GLint loc = glGetUniformLocation(program, name);
    if (loc < 0) GF_LOGW("uniform '%s' not found (optimised out?)", name);
    return loc;
}
