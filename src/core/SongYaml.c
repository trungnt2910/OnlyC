#include "core/Song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

typedef struct
{
    const char* key;
    char* value;
    size_t capacity;
    int* node;
} Field;

#define TEXT_FIELD(object, name)                                                                   \
    {.key = #name, .value = (object)->name, .capacity = sizeof((object)->name)}
#define NODE_FIELD(name, output) {.key = name, .node = output}

static int fail(char* error, size_t capacity, const char* message)
{
    if (capacity)
    {
        size_t length = strlen(message);
        if (length >= capacity)
        {
            length = capacity - 1;
        }
        memcpy(error, message, length);
        error[length] = 0;
    }
    return 0;
}

static int scalar(yaml_node_t* node, char* destination, size_t capacity)
{
    if (!node || node->type != YAML_SCALAR_NODE || node->data.scalar.length >= capacity ||
        memchr(node->data.scalar.value, 0, node->data.scalar.length))
    {
        return 0;
    }
    memcpy(destination, node->data.scalar.value, node->data.scalar.length);
    destination[node->data.scalar.length] = 0;
    return 1;
}

static int fields(yaml_document_t* document,
                  yaml_node_t* mapping,
                  Field* specs,
                  size_t count,
                  char* error,
                  size_t capacity)
{
    unsigned seen = 0;
    if (!mapping || mapping->type != YAML_MAPPING_NODE)
    {
        return fail(error, capacity, "Expected a YAML mapping.");
    }
    for (yaml_node_pair_t* entry = mapping->data.mapping.pairs.start;
         entry < mapping->data.mapping.pairs.top;
         ++entry)
    {
        char key[80];
        if (!scalar(yaml_document_get_node(document, entry->key), key, sizeof(key)))
        {
            return fail(error, capacity, "Invalid YAML key.");
        }
        size_t index = 0;
        while (index < count && strcmp(key, specs[index].key))
        {
            ++index;
        }
        if (index == count)
        {
            return fail(error, capacity, "Unknown YAML field. Check the sample schema.");
        }
        if (seen & (1u << index))
        {
            return fail(error, capacity, "Duplicate YAML field.");
        }
        seen |= 1u << index;
        if (specs[index].node)
        {
            *specs[index].node = entry->value;
        }
        else if (!scalar(yaml_document_get_node(document, entry->value),
                         specs[index].value,
                         specs[index].capacity))
        {
            return fail(error, capacity, "Field is too long or is not a text scalar.");
        }
    }
    return 1;
}

static int preflight(const char* text, size_t length, char* error, size_t error_size)
{
    yaml_parser_t parser;
    yaml_event_t event;
    int depth = 0, documents = 0, ok = 1, done = 0;
    size_t events = 0;
    if (!length || length > MAX_IMPORT)
    {
        return fail(error, error_size, "Choose a nonempty YAML file no larger than 1 MiB.");
    }
    if (!yaml_parser_initialize(&parser))
    {
        return fail(error, error_size, "Not enough memory.");
    }
    yaml_parser_set_input_string(&parser, (const unsigned char*)text, length);
    while (ok && !done)
    {
        if (!yaml_parser_parse(&parser, &event))
        {
            ok = fail(error, error_size, "Invalid YAML or UTF-8.");
            break;
        }
        if (event.type == YAML_MAPPING_START_EVENT || event.type == YAML_SEQUENCE_START_EVENT)
        {
            ++depth;
        }
        if (event.type == YAML_MAPPING_END_EVENT || event.type == YAML_SEQUENCE_END_EVENT)
        {
            --depth;
        }
        if (event.type == YAML_DOCUMENT_START_EVENT)
        {
            ++documents;
        }
        if (event.type == YAML_ALIAS_EVENT || depth > 16 || ++events > 16384 || documents > 1)
        {
            ok = fail(
                error, error_size, "Use one YAML document without aliases or deeply nested data.");
        }
        done = event.type == YAML_STREAM_END_EVENT;
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    return ok;
}

static int emit_write(void* data, unsigned char* text, size_t length)
{
    Buffer* buffer = data;
    buffer_append(buffer, (const char*)text, length);
    return !buffer->failed;
}

/* Only annotations use folded scalars in our emitted schema. Wrapping their
 * unindented content at single spaces preserves the decoded Markdown exactly.
 * Count UTF-8 characters, include YAML indentation and keep long tokens intact. */
static void wrap_annotation_line(Buffer* out, const char* line, size_t length)
{
    size_t start = 4, space = 0, column = 4;
    for (size_t i = start; i < length; ++i)
    {
        if (line[i] == ' ' && i > start && line[i - 1] != ' ' && line[i - 1] != '\t' &&
            i + 1 < length && line[i + 1] != ' ' && line[i + 1] != '\t')
        {
            space = i;
        }
        if (((unsigned char)line[i] & 0xc0) != 0x80)
        {
            ++column;
        }
        if (column > ONLYC_YAML_LINE_WIDTH && space)
        {
            buffer_puts(out, "    ");
            buffer_append(out, line + start, space - start);
            buffer_puts(out, "\n");
            start = space + 1;
            i = start - 1;
            column = 4;
            space = 0;
        }
    }
    buffer_puts(out, "    ");
    buffer_append(out, line + start, length - start);
}

static void write_wrapped_yaml(const Buffer* serialized, Buffer* out)
{
    int folded = 0;
    const char* line = serialized->data;
    while (*line)
    {
        size_t length = strcspn(line, "\n");
        if (length && (length < 4 || memcmp(line, "    ", 4)))
        {
            /* Keys are validated ASCII identifiers, with no colon or quotes. */
            const char* colon = memchr(line, ':', length);
            folded = length > 2 && line[0] == ' ' && line[1] == ' ' && line[2] != ' ' && colon &&
                     (size_t)(colon - line) + 2 < length && colon[1] == ' ' && colon[2] == '>';
        }
        if (folded && length > 4 && !memcmp(line, "    ", 4))
        {
            wrap_annotation_line(out, line, length);
        }
        else
        {
            buffer_append(out, line, length);
        }
        line += length;
        if (*line)
        {
            buffer_puts(out, "\n");
            ++line;
        }
    }
}

/* Normalize exported multiline text without changing the editor's source.
 * Keep indentation, blank lines and line counts; remove only line-end spaces
 * and tabs, and use LF for every line ending. */
static void normalize_multiline(const char* text, Buffer* out)
{
    while (*text)
    {
        const char* end = text + strcspn(text, "\r\n");
        const char* trimmed = end;
        while (trimmed > text && (trimmed[-1] == ' ' || trimmed[-1] == '\t'))
        {
            --trimmed;
        }
        buffer_append(out, text, (size_t)(trimmed - text));
        text = end;
        if (*text)
        {
            buffer_puts(out, "\n");
            if (*text++ == '\r' && *text == '\n')
            {
                ++text;
            }
        }
    }
}

static int has_indented_line(const char* text)
{
    while (*text)
    {
        if (*text == ' ' || *text == '\t')
        {
            return 1;
        }
        text += strcspn(text, "\n");
        if (*text)
        {
            ++text;
        }
    }
    return 0;
}

static int pair(yaml_document_t* document,
                int mapping,
                const char* key,
                const char* value,
                yaml_scalar_style_t style)
{
    Buffer normalized = {0};
    if (style == YAML_LITERAL_SCALAR_STYLE || style == YAML_FOLDED_SCALAR_STYLE)
    {
        normalize_multiline(value, &normalized);
        if (normalized.failed)
        {
            buffer_free(&normalized);
            return 0;
        }
        value = normalized.data ? normalized.data : "";
    }
    if (style == YAML_FOLDED_SCALAR_STYLE && !*value)
    {
        buffer_free(&normalized);
        return 0;
    }
    /* Folding an indented line can introduce significant newlines into code
     * or nested Markdown. Keep such annotations literal and unwrapped. */
    if (style == YAML_FOLDED_SCALAR_STYLE && has_indented_line(value))
    {
        style = YAML_LITERAL_SCALAR_STYLE;
    }
    int key_node = yaml_document_add_scalar(
        document, NULL, (const unsigned char*)key, -1, YAML_PLAIN_SCALAR_STYLE);
    int value_node =
        yaml_document_add_scalar(document, NULL, (const unsigned char*)value, -1, style);
    buffer_free(&normalized);
    return key_node && value_node &&
           yaml_document_append_mapping_pair(document, mapping, key_node, value_node);
}

static int parse_ranges(
    yaml_document_t* document, int node, Song* song, size_t section, char* error, size_t capacity)
{
    if (!node)
    {
        return 1;
    }
    yaml_node_t* sequence = yaml_document_get_node(document, node);
    if (!sequence || sequence->type != YAML_SEQUENCE_NODE)
    {
        return fail(error, capacity, "Section annotations must be a YAML sequence.");
    }
    for (yaml_node_item_t* item = sequence->data.sequence.items.start;
         item < sequence->data.sequence.items.top;
         ++item)
    {
        if (song->range_count == MAX_ANNOTATION_RANGES)
        {
            return fail(error, capacity, "At most 256 annotation ranges are supported.");
        }
        AnnotationRange* range = &song->ranges[song->range_count++];
        range->section = section;
        char first[16] = "", last[16] = "";
        Field specs[] = {
            {.key = "start", .value = first, .capacity = sizeof(first)},
            {.key = "end", .value = last, .capacity = sizeof(last)},
            {.key = "ref", .value = range->reference, .capacity = sizeof(range->reference)}};
        if (!fields(document, yaml_document_get_node(document, *item), specs, 3, error, capacity))
        {
            return 0;
        }
        if (!parse_line_number(first, &range->first_line) ||
            !parse_line_number(last, &range->last_line))
        {
            return fail(error,
                        capacity,
                        "Annotation start and end must be whole line numbers from 1 to 256.");
        }
    }
    return 1;
}

static int
parse_sections(yaml_document_t* document, int node, Song* song, char* error, size_t capacity)
{
    yaml_node_t* sequence = yaml_document_get_node(document, node);
    if (!sequence || sequence->type != YAML_SEQUENCE_NODE)
    {
        return fail(error, capacity, "Sections must be a YAML sequence.");
    }
    for (yaml_node_item_t* item = sequence->data.sequence.items.start;
         item < sequence->data.sequence.items.top;
         ++item)
    {
        if (song->count == MAX_SECTIONS)
        {
            return fail(error, capacity, "At most 32 sections are supported.");
        }
        size_t index = song->count++;
        Section* section = &song->sections[index];
        int ranges = 0;
        Field specs[] = {TEXT_FIELD(section, label),
                         TEXT_FIELD(section, original),
                         TEXT_FIELD(section, translated),
                         NODE_FIELD("annotations", &ranges)};
        if (!fields(document, yaml_document_get_node(document, *item), specs, 4, error, capacity) ||
            !parse_ranges(document, ranges, song, index, error, capacity))
        {
            return 0;
        }
    }
    return 1;
}

static int
parse_links(yaml_document_t* document, int node, Song* song, char* error, size_t capacity)
{
    if (!node)
    {
        return 1;
    }
    yaml_node_t* sequence = yaml_document_get_node(document, node);
    if (!sequence || sequence->type != YAML_SEQUENCE_NODE)
    {
        return fail(error, capacity, "Links must be a YAML sequence.");
    }
    for (yaml_node_item_t* item = sequence->data.sequence.items.start;
         item < sequence->data.sequence.items.top;
         ++item)
    {
        if (song->link_count == MAX_LINKS)
        {
            return fail(error, capacity, "At most 32 links are supported.");
        }
        SongLink* link = &song->links[song->link_count++];
        Field specs[] = {TEXT_FIELD(link, label), TEXT_FIELD(link, url)};
        if (!fields(document, yaml_document_get_node(document, *item), specs, 2, error, capacity))
        {
            return 0;
        }
    }
    return 1;
}

static int
parse_notes(yaml_document_t* document, int node, Song* song, char* error, size_t capacity)
{
    if (!node)
    {
        return 1;
    }
    yaml_node_t* mapping = yaml_document_get_node(document, node);
    if (!mapping || mapping->type != YAML_MAPPING_NODE)
    {
        return fail(error, capacity, "Annotations must be a mapping of keys to Markdown text.");
    }
    for (yaml_node_pair_t* entry = mapping->data.mapping.pairs.start;
         entry < mapping->data.mapping.pairs.top;
         ++entry)
    {
        if (song->annotation_count == MAX_ANNOTATIONS)
        {
            return fail(error, capacity, "At most 128 annotations are supported.");
        }
        Annotation* note = &song->annotations[song->annotation_count++];
        if (!scalar(yaml_document_get_node(document, entry->key), note->key, sizeof(note->key)) ||
            !scalar(yaml_document_get_node(document, entry->value), note->text, sizeof(note->text)))
        {
            return fail(
                error, capacity, "Annotation key or text is too long or is not a text scalar.");
        }
    }
    return 1;
}

int song_parse(Song* out, const char* text, size_t length, char* error, size_t capacity)
{
    if (!preflight(text, length, error, capacity))
    {
        return 0;
    }
    Song* song = calloc(1, sizeof(*song));
    yaml_parser_t parser;
    yaml_document_t document;
    if (!song || !yaml_parser_initialize(&parser))
    {
        free(song);
        return fail(error, capacity, "Not enough memory.");
    }
    yaml_parser_set_input_string(&parser, (const unsigned char*)text, length);
    if (!yaml_parser_load(&parser, &document))
    {
        yaml_parser_delete(&parser);
        free(song);
        return fail(error, capacity, "Unable to load YAML.");
    }
    char version[8] = "";
    int title = 0, links = 0, notes = 0, sections = 0;
    Field specs[] = {{.key = "version", .value = version, .capacity = sizeof(version)},
                     TEXT_FIELD(song, slug),
                     TEXT_FIELD(song, artist),
                     TEXT_FIELD(song, translator),
                     TEXT_FIELD(song, artwork),
                     TEXT_FIELD(song, color),
                     NODE_FIELD("title", &title),
                     NODE_FIELD("links", &links),
                     NODE_FIELD("annotations", &notes),
                     NODE_FIELD("sections", &sections)};
    int ok = fields(&document,
                    yaml_document_get_root_node(&document),
                    specs,
                    sizeof(specs) / sizeof(*specs),
                    error,
                    capacity);
    if (ok && strcmp(version, "1"))
    {
        ok = fail(error, capacity, "Expected schema version: 1.");
    }
    Field title_fields[] = {TEXT_FIELD(&song->title, original),
                            TEXT_FIELD(&song->title, translated),
                            TEXT_FIELD(&song->title, annotation)};
    if (ok)
    {
        ok = fields(&document,
                    yaml_document_get_node(&document, title),
                    title_fields,
                    3,
                    error,
                    capacity) &&
             parse_links(&document, links, song, error, capacity) &&
             parse_notes(&document, notes, song, error, capacity) &&
             parse_sections(&document, sections, song, error, capacity) &&
             song_validate(song, error, capacity);
    }
    if (ok)
    {
        memcpy(out, song, sizeof(*out));
    }
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    free(song);
    return ok;
}

static int collection(yaml_document_t* document, int parent, const char* key, int sequence)
{
    int name = yaml_document_add_scalar(
        document, NULL, (const unsigned char*)key, -1, YAML_PLAIN_SCALAR_STYLE);
    int node = sequence ? yaml_document_add_sequence(document, NULL, YAML_BLOCK_SEQUENCE_STYLE)
                        : yaml_document_add_mapping(document, NULL, YAML_BLOCK_MAPPING_STYLE);
    return name && node && yaml_document_append_mapping_pair(document, parent, name, node) ? node
                                                                                           : 0;
}

int song_export(const Song* song, Buffer* out)
{
    char error[256];
    yaml_document_t document;
    yaml_emitter_t emitter;
    if (!song_validate(song, error, sizeof(error)) ||
        !yaml_document_initialize(&document, NULL, NULL, NULL, 1, 1))
    {
        return 0;
    }
    int root = yaml_document_add_mapping(&document, NULL, YAML_BLOCK_MAPPING_STYLE);
    int ok = root && pair(&document, root, "version", "1", YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
             pair(&document, root, "slug", song->slug, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    int title = ok ? collection(&document, root, "title", 0) : 0;
    ok =
        ok && title &&
        pair(&document, title, "original", song->title.original, YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
        pair(&document,
             title,
             "translated",
             song->title.translated,
             YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
        pair(&document,
             title,
             "annotation",
             song->title.annotation,
             YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
        pair(&document, root, "artist", song->artist, YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
        pair(&document, root, "translator", song->translator, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    if (song->artwork[0])
    {
        ok = ok && pair(&document, root, "artwork", song->artwork, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    }
    if (song->color[0])
    {
        ok = ok && pair(&document, root, "color", song->color, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    }
    int links = ok ? collection(&document, root, "links", 1) : 0;
    ok = ok && links;
    for (size_t i = 0; i < song->link_count && ok; ++i)
    {
        int node = yaml_document_add_mapping(&document, NULL, YAML_BLOCK_MAPPING_STYLE);
        ok =
            node && yaml_document_append_sequence_item(&document, links, node) &&
            pair(&document, node, "label", song->links[i].label, YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
            pair(&document, node, "url", song->links[i].url, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    }
    int notes = ok ? collection(&document, root, "annotations", 0) : 0;
    ok = ok && notes;
    for (size_t i = 0; i < song->annotation_count && ok; ++i)
    {
        ok = pair(&document,
                  notes,
                  song->annotations[i].key,
                  song->annotations[i].text,
                  YAML_FOLDED_SCALAR_STYLE);
    }
    int sections = ok ? collection(&document, root, "sections", 1) : 0;
    ok = ok && sections;
    for (size_t i = 0; i < song->count && ok; ++i)
    {
        const Section* section = &song->sections[i];
        int node = yaml_document_add_mapping(&document, NULL, YAML_BLOCK_MAPPING_STYLE);
        ok = node && yaml_document_append_sequence_item(&document, sections, node) &&
             pair(&document, node, "label", section->label, YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
             pair(&document, node, "original", section->original, YAML_LITERAL_SCALAR_STYLE) &&
             pair(&document, node, "translated", section->translated, YAML_LITERAL_SCALAR_STYLE);
        int ranges = ok ? collection(&document, node, "annotations", 1) : 0;
        ok = ok && ranges;
        /* Export bindings in lyric order, independently of definition order. */
        for (size_t line = 1; line <= lyrics_line_count(section->translated) && ok; ++line)
        {
            int index = annotation_range_for_line(song, i, line);
            if (index < 0 || song->ranges[index].first_line != line)
            {
                continue;
            }
            const AnnotationRange* range = &song->ranges[index];
            int entry = yaml_document_add_mapping(&document, NULL, YAML_BLOCK_MAPPING_STYLE);
            char first[16], last[16];
            snprintf(first, sizeof(first), "%zu", range->first_line);
            snprintf(last, sizeof(last), "%zu", range->last_line);
            ok = entry && yaml_document_append_sequence_item(&document, ranges, entry) &&
                 pair(&document, entry, "start", first, YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
                 pair(&document, entry, "end", last, YAML_DOUBLE_QUOTED_SCALAR_STYLE) &&
                 pair(&document, entry, "ref", range->reference, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
        }
    }
    if (!ok || !yaml_emitter_initialize(&emitter))
    {
        yaml_document_delete(&document);
        return 0;
    }
    Buffer serialized = {0};
    yaml_emitter_set_output(&emitter, emit_write, &serialized);
    yaml_emitter_set_unicode(&emitter, 1);
    /* libyaml's preferred width can overrun by a word. Apply the exact limit
     * to annotation content after serialization, without splitting tokens. */
    yaml_emitter_set_width(&emitter, -1);
    yaml_emitter_set_indent(&emitter, 2);
    yaml_emitter_set_break(&emitter, YAML_LN_BREAK);
    ok = yaml_emitter_dump(&emitter, &document);
    yaml_emitter_delete(&emitter);
    if (ok && !serialized.failed)
    {
        write_wrapped_yaml(&serialized, out);
    }
    ok = ok && !serialized.failed && !out->failed;
    buffer_free(&serialized);
    return ok;
}
