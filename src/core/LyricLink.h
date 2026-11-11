#ifndef ONLYC_CORE_LYRICLINK_H
#define ONLYC_CORE_LYRICLINK_H

#include "core/Song.h"

typedef struct
{
    int section;
    size_t line;
} LyricTarget;

/* Public line numbers are one-based across verses, excluding labels/title.
 * Resolve the first overlapping item; annotated lines resolve to their range start.
 * On failure, leave the output untouched. */
int lyric_link_resolve(const Song* song, const char* fragment, LyricTarget* target);
int lyric_link_format(const Song* song, int section, size_t line, char* text, size_t capacity);

#endif
