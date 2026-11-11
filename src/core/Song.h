#ifndef ONLYC_CORE_SONG_H
#define ONLYC_CORE_SONG_H

#include <stddef.h>

#define MAX_SECTIONS 32
#define MAX_ANNOTATIONS 128
#define MAX_ANNOTATION_RANGES 256
#define MAX_LINKS 32
#define MAX_LINES 256
#define MAX_IMPORT (1024u * 1024u)

/* The title is selectable like a lyric section; -1 means no selection. */
#define TITLE_SECTION (-2)

typedef struct
{
    char original[160];
    char translated[160];
    char annotation[80];
} SongTitle;

typedef struct
{
    char label[80];
    char url[1024];
} SongLink;

typedef struct
{
    char label[80];
    char original[4096];
    char translated[4096];
} Section;

typedef struct
{
    char key[80];
    char text[8192];
} Annotation;

/* A binding is independent of annotation text. Multiple bindings may reuse a key.
 * Section is zero-based internally; line ranges are one-based and inclusive. */
typedef struct
{
    size_t section;
    size_t first_line;
    size_t last_line;
    char reference[80];
} AnnotationRange;

typedef struct
{
    char slug[80];
    SongTitle title;
    char artist[160];
    char translator[160];
    char artwork[1024];
    char color[8];
    size_t link_count;
    SongLink links[MAX_LINKS];
    size_t count;
    Section sections[MAX_SECTIONS];
    size_t annotation_count;
    Annotation annotations[MAX_ANNOTATIONS];
    size_t range_count;
    AnnotationRange ranges[MAX_ANNOTATION_RANGES];
} Song;

typedef struct
{
    char* data;
    size_t len;
    size_t capacity;
    int failed;
} Buffer;

void buffer_append(Buffer* b, const char* s, size_t n);
void buffer_puts(Buffer* b, const char* s);
void buffer_html(Buffer* b, const char* s, size_t n);
void buffer_free(Buffer* b);
int song_parse(Song* out, const char* yaml, size_t len, char* error, size_t error_size);
int song_export(const Song* song, Buffer* out);
int song_validate(const Song* song, char* error, size_t error_size);
int safe_url(const char* url);
const char* song_title(const Song* song);
const char* song_description(const Song* song);
int annotation_by_key(const Song* song, const char* key);
int annotation_range_for_line(const Song* song, size_t section, size_t line);
int annotation_for_line(const Song* song, size_t section, size_t line);
int annotation_for_selection(const Song* song, int section, size_t line);
size_t lyrics_line_count(const char* text);
void lyrics_line_range(const char* text, size_t first, size_t last, Buffer* out);
int parse_line_number(const char* text, size_t* value);
void song_remove_annotation(Song* song, size_t index);
void song_remove_range(Song* song, size_t index);
void song_remove_section(Song* song, size_t index);
#endif
