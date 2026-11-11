#ifndef ONLYC_CORE_ARTWORK_H
#define ONLYC_CORE_ARTWORK_H

#include <stdbool.h>
#include <stddef.h>

#define ARTWORK_SAMPLE_SIDE 64
#define ARTWORK_SAMPLE_PIXELS (ARTWORK_SAMPLE_SIDE * ARTWORK_SAMPLE_SIDE)
#define ARTWORK_SAMPLE_BYTES (4 * ARTWORK_SAMPLE_PIXELS)

/* Quantize opaque sRGB samples, then choose the largest populated swatch.
 * Transparent pixels and near-black/white text are excluded where possible. */
bool artwork_color(const unsigned char* rgba, size_t pixels, char color[8]);

#endif
