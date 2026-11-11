#include "core/LyricLink.h"

#include <stdio.h>
#include <string.h>

static int number(const char** cursor, size_t* result)
{
    const char* p = *cursor;
    size_t value = 0;
    if (*p < '0' || *p > '9')
    {
        return 0;
    }
    while (*p >= '0' && *p <= '9')
    {
        unsigned digit = (unsigned)(*p++ - '0');
        if (value > (MAX_SECTIONS * MAX_LINES - digit) / 10)
        {
            return 0;
        }
        value = value * 10 + digit;
    }
    if (!value)
    {
        return 0;
    }
    *cursor = p;
    *result = value;
    return 1;
}

int lyric_link_resolve(const Song* song, const char* fragment, LyricTarget* target)
{
    if (!song || !fragment || !target)
    {
        return 0;
    }
    if (!strcmp(fragment, "#title"))
    {
        *target = (LyricTarget){TITLE_SECTION, 1};
        return 1;
    }
    if (strncmp(fragment, "#L", 2))
    {
        return 0;
    }
    const char* cursor = fragment + 2;
    size_t first, last;
    if (!number(&cursor, &first))
    {
        return 0;
    }
    last = first;
    if (*cursor == '-')
    {
        ++cursor;
        if (*cursor++ != 'L' || !number(&cursor, &last))
        {
            return 0;
        }
    }
    if (*cursor || last < first)
    {
        return 0;
    }
    size_t offset = 0;
    for (size_t i = 0; i < song->count; ++i)
    {
        size_t count = lyrics_line_count(song->sections[i].translated);
        if (first > offset && first <= offset + count)
        {
            size_t line = first - offset;
            int range = annotation_range_for_line(song, i, line);
            *target = (LyricTarget){(int)i, range < 0 ? line : song->ranges[range].first_line};
            return 1;
        }
        offset += count;
    }
    return 0;
}

int lyric_link_format(const Song* song, int section, size_t line, char* text, size_t capacity)
{
    if (!song || !text || !capacity)
    {
        return 0;
    }
    text[0] = 0;
    if (section == TITLE_SECTION)
    {
        int length = snprintf(text, capacity, "#title");
        if (length >= 0 && (size_t)length < capacity)
        {
            return 1;
        }
        text[0] = 0;
        return 0;
    }
    if (section < 0)
    {
        return 1;
    }
    if ((size_t)section >= song->count || !line ||
        line > lyrics_line_count(song->sections[section].translated))
    {
        return 0;
    }
    size_t offset = 0;
    for (int i = 0; i < section; ++i)
    {
        offset += lyrics_line_count(song->sections[i].translated);
    }
    int range = annotation_range_for_line(song, (size_t)section, line);
    size_t first = range < 0 ? line : song->ranges[range].first_line;
    size_t last = range < 0 ? line : song->ranges[range].last_line;
    int length = first == last
                     ? snprintf(text, capacity, "#L%zu", offset + first)
                     : snprintf(text, capacity, "#L%zu-L%zu", offset + first, offset + last);
    if (length < 0 || (size_t)length >= capacity)
    {
        text[0] = 0;
        return 0;
    }
    return 1;
}
