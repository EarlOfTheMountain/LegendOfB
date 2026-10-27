#include "gf_assets.h"
#include "gf_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char s_root[512] = "assets";

static bool is_dir(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

bool gf_assets_init(const char *root)
{
    if (root) {
        if (!is_dir(root)) {
            GF_LOGE("asset root '%s' is not a directory", root);
            return false;
        }
        snprintf(s_root, sizeof s_root, "%s", root);
        return true;
    }

    if (is_dir("assets")) {
        snprintf(s_root, sizeof s_root, "assets");
        return true;
    }

    char exe[400];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = '\0';
            char cand[512];
            snprintf(cand, sizeof cand, "%s/assets", exe);
            if (is_dir(cand)) { snprintf(s_root, sizeof s_root, "%s", cand); return true; }
            snprintf(cand, sizeof cand, "%s/../assets", exe);
            if (is_dir(cand)) { snprintf(s_root, sizeof s_root, "%s", cand); return true; }
        }
    }

    GF_LOGE("could not find an 'assets' directory (cwd, exe dir, exe dir/..)");
    return false;
}

const char *gf_asset_root(void) { return s_root; }

void gf_asset_path(char *out, size_t n, const char *rel)
{
    snprintf(out, n, "%s/%s", s_root, rel);
}

void *gf_asset_read(const char *rel, size_t *size_out)
{
    char path[1024];
    gf_asset_path(path, sizeof path, rel);

    FILE *f = fopen(path, "rb");
    if (!f) {
        GF_LOGE("cannot open asset '%s'", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    unsigned char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) {
        GF_LOGE("short read on '%s'", path);
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    if (size_out) *size_out = (size_t)len;
    return buf;
}
