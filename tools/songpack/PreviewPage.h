#ifndef ONLYC_SONGPACK_PREVIEW_PAGE_H
#define ONLYC_SONGPACK_PREVIEW_PAGE_H

#include <stdbool.h>

#include "core/Song.h"

/* A null song emits the index metadata. The body is shared by every page. */
bool preview_page_render(const Song* song, const char* origin, Buffer* output);

#endif
