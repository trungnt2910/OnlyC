#ifndef ONLYC_APP_MODEL_EDITORSTATE_H
#define ONLYC_APP_MODEL_EDITORSTATE_H

#include "core/Song.h"

typedef enum
{
    EDITOR_TITLE,
    EDITOR_TITLE_TRANSLATED,
    EDITOR_TITLE_ANNOTATION,
    EDITOR_ARTIST,
    EDITOR_TRANSLATOR,
    EDITOR_SLUG,
    EDITOR_ARTWORK,
    EDITOR_COLOR,
    EDITOR_LABEL,
    EDITOR_ORIGINAL,
    EDITOR_TRANSLATED,
    EDITOR_ANNOTATION_REF,
    EDITOR_ANNOTATION,
    EDITOR_ANNOTATION_START,
    EDITOR_ANNOTATION_END,
    EDITOR_FIELD_COUNT
} EditorField;

typedef struct
{
    Buffer label;
    Buffer url;
} EditorLink;

/* Raw values survive failed validation and detached views. */
typedef struct
{
    Buffer fields[EDITOR_FIELD_COUNT];
    EditorField changed_field;
    size_t changed_link;
    int changed_link_url;
    size_t link_count;
    EditorLink links[MAX_LINKS];
} EditorState;

const char* editor_state_text(const EditorState* state, EditorField field);
int editor_state_set(EditorState* state, EditorField field, const char* text);
int editor_state_load(EditorState* state, const Song* song, int section, size_t line);
void editor_state_destroy(EditorState* state);
size_t editor_field_capacity(EditorField field);
int editor_link_set(EditorState* state, size_t index, int url, const char* text);
void editor_link_remove(EditorState* state, size_t index);

#endif
