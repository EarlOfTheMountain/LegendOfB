#ifndef GF_ASSETS_H
#define GF_ASSETS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Assets live under a root directory (default "assets"), searched in:
 *   ./assets, <exe dir>/assets, <exe dir>/../assets
 * All asset paths in the API are relative to the root, e.g.
 *   "shaders/sprite.vert", "images/ship.tga", "sounds/boom.wav".
 */
bool        gf_assets_init(const char *root_or_null);
const char *gf_asset_root(void);
void        gf_asset_path(char *out, size_t n, const char *rel);

/* Reads a whole asset. Result is malloc'd and NUL-terminated (size excludes
 * the terminator), so it is safe to use directly as shader source. Caller
 * frees. Returns NULL (and logs) on failure. */
void       *gf_asset_read(const char *rel, size_t *size_out);

#endif
