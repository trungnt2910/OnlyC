#include "core/Song.h"

#include <stdio.h>
#include <string.h>

#include "core/Color.h"

static int fail(char* error, size_t error_size, const char* message)
{
    if (error_size)
    {
        snprintf(error, error_size, "%s", message);
    }
    return 0;
}

int safe_url(const char* url)
{
    if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))
    {
        return 0;
    }
    const char* host = strstr(url, "://") + 3;
    if (!*host || *host == '/' || *host == '?' || *host == '#')
    {
        return 0;
    }
    for (const unsigned char* p = (const unsigned char*)url; *p; ++p)
    {
        if (*p <= 32 || *p == 127 || *p == '\\')
        {
            return 0;
        }
    }
    return 1;
}

const char* song_title(const Song* song)
{
    return song->title.translated[0] ? song->title.translated : song->title.original;
}

const char* song_description(const Song* song)
{
    int index = annotation_by_key(song, song->title.annotation);
    return index < 0 ? "" : song->annotations[index].text;
}

static int valid_key(const char* text)
{
    if (!text[0])
    {
        return 0;
    }
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
    {
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= 'A' && *p <= 'Z') && !(*p >= '0' && *p <= '9') &&
            *p != '-' && *p != '_')
        {
            return 0;
        }
    }
    return 1;
}

int song_validate(const Song* song, char* error, size_t error_size)
{
    if (!song->slug[0] || !song->title.original[0] || !song->artist[0])
    {
        return fail(error, error_size, "Slug, original title and artist are required.");
    }
    for (const char* p = song->slug; *p; ++p)
    {
        if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') && *p != '-')
        {
            return fail(
                error, error_size, "Slug must contain only lowercase letters, digits and hyphens.");
        }
    }
    if (strpbrk(song->title.original, "\r\n") || strpbrk(song->title.translated, "\r\n"))
    {
        return fail(error, error_size, "Titles must be single lines.");
    }
    if (song->link_count > MAX_LINKS)
    {
        return fail(error, error_size, "At most 32 links are supported.");
    }
    for (size_t i = 0; i < song->link_count; ++i)
    {
        if (!song->links[i].label[0] || !safe_url(song->links[i].url))
        {
            return fail(error, error_size, "Each link needs a label and an HTTP or HTTPS URL.");
        }
    }
    if (song->artwork[0] && !safe_url(song->artwork))
    {
        return fail(error, error_size, "Artwork must be an HTTP or HTTPS URL.");
    }
    Color color;
    if (song->color[0] && !color_parse(song->color, &color))
    {
        return fail(error, error_size, "Color must be a #RRGGBB value.");
    }
    if (!song->count || song->count > MAX_SECTIONS)
    {
        return fail(error, error_size, "A song needs between 1 and 32 sections.");
    }
    for (size_t i = 0; i < song->count; ++i)
    {
        const Section* section = &song->sections[i];
        if (!section->label[0])
        {
            return fail(error, error_size, "Each section needs a label.");
        }
        size_t lines = lyrics_line_count(section->translated);
        if (lines > MAX_LINES)
        {
            return fail(error, error_size, "At most 256 lines per section are supported.");
        }
        if (lines != lyrics_line_count(section->original))
        {
            return fail(error,
                        error_size,
                        "Original and translated lyrics must have the same number of lines in each "
                        "section.");
        }
    }
    if (song->annotation_count > MAX_ANNOTATIONS || song->range_count > MAX_ANNOTATION_RANGES)
    {
        return fail(error, error_size, "At most 128 annotations and 256 ranges are supported.");
    }
    for (size_t i = 0; i < song->annotation_count; ++i)
    {
        const Annotation* note = &song->annotations[i];
        if (!valid_key(note->key) || !note->text[0])
        {
            return fail(
                error,
                error_size,
                "Each annotation needs a key (letters, digits, hyphens or underscores) and text.");
        }
        for (size_t j = 0; j < i; ++j)
        {
            if (!strcmp(note->key, song->annotations[j].key))
            {
                return fail(error, error_size, "Annotation keys must be unique.");
            }
        }
    }
    if (song->title.annotation[0] && annotation_by_key(song, song->title.annotation) < 0)
    {
        return fail(error, error_size, "Title annotation references an unknown key.");
    }
    for (size_t i = 0; i < song->range_count; ++i)
    {
        const AnnotationRange* range = &song->ranges[i];
        if (range->section >= song->count || !range->first_line ||
            range->last_line < range->first_line ||
            range->last_line > lyrics_line_count(song->sections[range->section].translated))
        {
            return fail(error,
                        error_size,
                        "Annotation ranges must reference existing whole lines, in order.");
        }
        if (annotation_by_key(song, range->reference) < 0)
        {
            return fail(error, error_size, "Annotation range references an unknown key.");
        }
        for (size_t j = 0; j < i; ++j)
        {
            const AnnotationRange* other = &song->ranges[j];
            if (range->section == other->section && range->first_line <= other->last_line &&
                other->first_line <= range->last_line)
            {
                return fail(
                    error, error_size, "Annotation ranges cannot overlap within a section.");
            }
        }
    }
    if (error_size)
    {
        error[0] = 0;
    }
    return 1;
}
