#include "app/model/EditorState.h"

#include <stdio.h>
#include <string.h>

size_t editor_field_capacity(EditorField field)
{
    static const size_t capacities[EDITOR_FIELD_COUNT] = {
        [EDITOR_TITLE] = sizeof(((SongTitle*)0)->original),
        [EDITOR_TITLE_TRANSLATED] = sizeof(((SongTitle*)0)->translated),
        [EDITOR_TITLE_ANNOTATION] = sizeof(((SongTitle*)0)->annotation),
        [EDITOR_ARTIST] = sizeof(((Song*)0)->artist),
        [EDITOR_TRANSLATOR] = sizeof(((Song*)0)->translator),
        [EDITOR_SLUG] = sizeof(((Song*)0)->slug),
        [EDITOR_ARTWORK] = sizeof(((Song*)0)->artwork),
        [EDITOR_COLOR] = sizeof(((Song*)0)->color),
        [EDITOR_LABEL] = sizeof(((Section*)0)->label),
        [EDITOR_ORIGINAL] = sizeof(((Section*)0)->original),
        [EDITOR_TRANSLATED] = sizeof(((Section*)0)->translated),
        [EDITOR_ANNOTATION_REF] = sizeof(((Annotation*)0)->key),
        [EDITOR_ANNOTATION] = sizeof(((Annotation*)0)->text),
        [EDITOR_ANNOTATION_START] = 16,
        [EDITOR_ANNOTATION_END] = 16};
    return field >= 0 && field < EDITOR_FIELD_COUNT ? capacities[field] : 0;
}

const char* editor_state_text(const EditorState* state, EditorField field)
{
    if (field < 0 || field >= EDITOR_FIELD_COUNT)
    {
        return "";
    }
    return state->fields[field].data ? state->fields[field].data : "";
}

int editor_state_set(EditorState* state, EditorField field, const char* text)
{
    if (field < 0 || field >= EDITOR_FIELD_COUNT || !text)
    {
        return 0;
    }
    Buffer next = {0};
    buffer_puts(&next, text);
    if (next.failed)
    {
        buffer_free(&next);
        return 0;
    }
    buffer_free(&state->fields[field]);
    state->fields[field] = next;
    state->changed_field = field;
    return 1;
}

int editor_state_load(EditorState* state, const Song* song, int section, size_t line)
{
    EditorState next = {.changed_field = EDITOR_FIELD_COUNT};
    int ok = 1;
#define LOAD(field, value) ok &= editor_state_set(&next, field, value)
    LOAD(EDITOR_TITLE, song->title.original);
    LOAD(EDITOR_TITLE_TRANSLATED, song->title.translated);
    LOAD(EDITOR_TITLE_ANNOTATION, song->title.annotation);
    LOAD(EDITOR_ARTIST, song->artist);
    LOAD(EDITOR_TRANSLATOR, song->translator);
    LOAD(EDITOR_SLUG, song->slug);
    LOAD(EDITOR_ARTWORK, song->artwork);
    LOAD(EDITOR_COLOR, song->color);
    next.link_count = song->link_count;
    for (size_t i = 0; i < song->link_count; ++i)
    {
        ok &= editor_link_set(&next, i, 0, song->links[i].label);
        ok &= editor_link_set(&next, i, 1, song->links[i].url);
    }
    int note_index = annotation_for_selection(song, section, line);
    const Annotation* annotation = note_index < 0 ? NULL : &song->annotations[note_index];
    if (section == TITLE_SECTION)
    {
        LOAD(EDITOR_ANNOTATION_REF, song->title.annotation);
        LOAD(EDITOR_ANNOTATION, annotation ? annotation->text : "");
    }
    if (section >= 0 && (size_t)section < song->count)
    {
        const Section* selected = &song->sections[section];
        int index = annotation_range_for_line(song, (size_t)section, line);
        const AnnotationRange* range = index < 0 ? NULL : &song->ranges[index];
        char first[16], last[16];
        snprintf(first, sizeof(first), "%zu", range ? range->first_line : line);
        snprintf(last, sizeof(last), "%zu", range ? range->last_line : line);
        LOAD(EDITOR_LABEL, selected->label);
        LOAD(EDITOR_ORIGINAL, selected->original);
        LOAD(EDITOR_TRANSLATED, selected->translated);
        LOAD(EDITOR_ANNOTATION_REF, range ? range->reference : "");
        LOAD(EDITOR_ANNOTATION, annotation ? annotation->text : "");
        LOAD(EDITOR_ANNOTATION_START, first);
        LOAD(EDITOR_ANNOTATION_END, last);
    }
#undef LOAD
    if (!ok)
    {
        editor_state_destroy(&next);
        return 0;
    }
    editor_state_destroy(state);
    next.changed_field = EDITOR_FIELD_COUNT;
    *state = next;
    return 1;
}

void editor_state_destroy(EditorState* state)
{
    for (int field = 0; field < EDITOR_FIELD_COUNT; ++field)
    {
        buffer_free(&state->fields[field]);
    }
    for (size_t i = 0; i < state->link_count; ++i)
    {
        buffer_free(&state->links[i].label);
        buffer_free(&state->links[i].url);
    }
    memset(state, 0, sizeof(*state));
    state->changed_field = EDITOR_FIELD_COUNT;
}

int editor_link_set(EditorState* state, size_t index, int url, const char* text)
{
    if (index >= state->link_count)
    {
        return 0;
    }
    Buffer* field = url ? &state->links[index].url : &state->links[index].label;
    if (!text)
    {
        field->failed = 1;
        return 0;
    }
    Buffer next = {0};
    buffer_puts(&next, text);
    if (next.failed)
    {
        buffer_free(&next);
        field->failed = 1;
        return 0;
    }
    buffer_free(field);
    *field = next;
    return 1;
}

void editor_link_remove(EditorState* state, size_t index)
{
    if (index >= state->link_count)
    {
        return;
    }
    buffer_free(&state->links[index].label);
    buffer_free(&state->links[index].url);
    memmove(&state->links[index],
            &state->links[index + 1],
            (state->link_count - index - 1) * sizeof(EditorLink));
    memset(&state->links[--state->link_count], 0, sizeof(EditorLink));
}
