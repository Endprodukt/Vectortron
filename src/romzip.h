/* romzip.h — one file out of a MAME ROM set (.zip), or a loose ROM file. */
#ifndef VT_ROMZIP_H
#define VT_ROMZIP_H

#include <stddef.h>

/* The first of `names` (NULL-terminated list, any case, any folder inside
 * the zip) found in the zip. malloc'ed; NULL if none. */
unsigned char *romzip_extract(const char *zip_path, const char *const *names, size_t *size);

/* A .zip: as romzip_extract. Anything else: the whole file. */
unsigned char *romzip_load(const char *path, const char *const *names, size_t *size);

#endif
