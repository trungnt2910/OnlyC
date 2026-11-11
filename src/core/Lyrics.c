#include "core/Song.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t lyrics_line_count(const char* text)
{
    size_t count = 1;
    for (const char* cursor = text; *cursor; ++cursor)
    {
        if (*cursor == '\n' && cursor[1])
        {
            ++count;
        }
    }
    return count;
}

void lyrics_line_range(const char* text, size_t first, size_t last, Buffer* out)
{
    size_t line = 1;
    const char* start = text;
    if (!first || last < first)
    {
        return;
    }
    while (*start && line <= last)
    {
        const char* end = strchr(start, '\n');
        if (!end)
        {
            end = start + strlen(start);
        }
        const char* content_end = end;
        if (content_end > start && content_end[-1] == '\r')
        {
            --content_end;
        }
        if (line >= first)
        {
            if (line > first)
            {
                buffer_puts(out, "\n");
            }
            buffer_append(out, start, (size_t)(content_end - start));
        }
        if (!*end)
        {
            break;
        }
        start = end + 1;
        ++line;
    }
}

int parse_line_number(const char* text, size_t* value)
{
    size_t number = 0;
    if (!*text)
    {
        return 0;
    }
    for (const char* cursor = text; *cursor; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
        {
            return 0;
        }
        number = number * 10 + (size_t)(*cursor - '0');
        if (number > MAX_LINES)
        {
            return 0;
        }
    }
    if (!number)
    {
        return 0;
    }
    *value = number;
    return 1;
}

int annotation_by_key(const Song* song, const char* key)
{
    for (size_t i = 0; i < song->annotation_count; ++i)
    {
        if (!strcmp(song->annotations[i].key, key))
        {
            return (int)i;
        }
    }
    return -1;
}

int annotation_range_for_line(const Song* song, size_t section, size_t line)
{
    for (size_t i = 0; i < song->range_count; ++i)
    {
        const AnnotationRange* range = &song->ranges[i];
        if (range->section == section && line >= range->first_line && line <= range->last_line)
        {
            return (int)i;
        }
    }
    return -1;
}

int annotation_for_line(const Song* song, size_t section, size_t line)
{
    int index = annotation_range_for_line(song, section, line);
    return index < 0 ? -1 : annotation_by_key(song, song->ranges[index].reference);
}

int annotation_for_selection(const Song* song, int section, size_t line)
{
    return section == TITLE_SECTION ? annotation_by_key(song, song->title.annotation)
           : section < 0            ? -1
                                    : annotation_for_line(song, (size_t)section, line);
}

void song_remove_range(Song* song, size_t index)
{
    if (index >= song->range_count)
    {
        return;
    }
    memmove(&song->ranges[index],
            &song->ranges[index + 1],
            (song->range_count - index - 1) * sizeof(AnnotationRange));
    memset(&song->ranges[--song->range_count], 0, sizeof(AnnotationRange));
}

void song_remove_annotation(Song* song, size_t index)
{
    if (index >= song->annotation_count)
    {
        return;
    }
    const char* key = song->annotations[index].key;
    if (!strcmp(song->title.annotation, key))
    {
        song->title.annotation[0] = 0;
    }
    for (size_t i = 0; i < song->range_count;)
    {
        if (!strcmp(song->ranges[i].reference, key))
        {
            song_remove_range(song, i);
        }
        else
        {
            ++i;
        }
    }
    memmove(&song->annotations[index],
            &song->annotations[index + 1],
            (song->annotation_count - index - 1) * sizeof(Annotation));
    memset(&song->annotations[--song->annotation_count], 0, sizeof(Annotation));
}

void song_remove_section(Song* song, size_t index)
{
    if (index >= song->count)
    {
        return;
    }
    for (size_t i = 0; i < song->range_count;)
    {
        AnnotationRange* range = &song->ranges[i];
        if (range->section == index)
        {
            song_remove_range(song, i);
        }
        else
        {
            if (range->section > index)
            {
                --range->section;
            }
            ++i;
        }
    }
    memmove(&song->sections[index],
            &song->sections[index + 1],
            (song->count - index - 1) * sizeof(Section));
    memset(&song->sections[--song->count], 0, sizeof(Section));
}
