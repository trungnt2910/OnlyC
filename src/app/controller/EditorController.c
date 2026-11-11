#include "app/controller/ControllerInternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strings/Resources.h"

void controller_refresh_dirty(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    state->has_unexported_changes = 0;
    for (int i = 0; i <= (int)state->catalog.count; ++i)
    {
        state->has_unexported_changes |= state->song_dirty[i];
    }
}

static int commit_annotation(
    ApplicationController* controller, Song* next, size_t* next_line, char* error, size_t capacity)
{
    ApplicationState* state = controller->state;
    const char* text = editor_state_text(&state->editor, EDITOR_ANNOTATION);
    const char* reference = editor_state_text(&state->editor, EDITOR_ANNOTATION_REF);
    int title = state->selected_section == TITLE_SECTION;
    int range_index = title ? -1
                            : annotation_range_for_line(
                                  next, (size_t)state->selected_section, state->selected_line);
    if (!text[0] && !reference[0])
    {
        if (title)
        {
            next->title.annotation[0] = 0;
        }
        else if (range_index >= 0)
        {
            song_remove_range(next, (size_t)range_index);
        }
        return 1;
    }
    char key[80];
    snprintf(key, sizeof(key), "%s", reference);
    if (!key[0])
    {
        size_t suffix = 1;
        do
        {
            snprintf(key, sizeof(key), "note-%zu", suffix++);
        } while (annotation_by_key(next, key) >= 0);
    }
    int index = annotation_by_key(next, key);
    if (index < 0)
    {
        if (next->annotation_count == MAX_ANNOTATIONS)
        {
            snprintf(error, capacity, "%s", ONLYC_STRING_ERROR_MAX_ANNOTATIONS);
            return 0;
        }
        index = (int)next->annotation_count++;
        strcpy(next->annotations[index].key, key);
    }
    memset(next->annotations[index].text, 0, sizeof(next->annotations[index].text));
    strcpy(next->annotations[index].text, text);
    if (title)
    {
        memset(next->title.annotation, 0, sizeof(next->title.annotation));
        strcpy(next->title.annotation, key);
        return 1;
    }
    size_t first, last;
    if (!parse_line_number(editor_state_text(&state->editor, EDITOR_ANNOTATION_START), &first) ||
        !parse_line_number(editor_state_text(&state->editor, EDITOR_ANNOTATION_END), &last))
    {
        snprintf(error, capacity, "%s", ONLYC_STRING_ERROR_ANNOTATION_RANGE);
        return 0;
    }
    if (range_index < 0)
    {
        if (next->range_count == MAX_ANNOTATION_RANGES)
        {
            snprintf(error, capacity, "%s", ONLYC_STRING_ERROR_MAX_RANGES);
            return 0;
        }
        range_index = (int)next->range_count++;
    }
    AnnotationRange* range = &next->ranges[range_index];
    range->section = (size_t)state->selected_section;
    range->first_line = first;
    range->last_line = last;
    memset(range->reference, 0, sizeof(range->reference));
    strcpy(range->reference, key);
    *next_line = first;
    return 1;
}

static int copy_field(const EditorState* editor, EditorField field, char* text, size_t capacity)
{
    const char* value = editor_state_text(editor, field);
    if (editor->fields[field].failed || strlen(value) >= capacity)
    {
        return 0;
    }
    strcpy(text, value);
    return 1;
}

void controller_present(ApplicationController* controller, ApplicationChange change)
{
    ApplicationState* state = controller->state;
    int request_artwork = controller_prepare_artwork(controller);
    if (state->editing && !state->pending_changes &&
        !editor_state_load(&state->editor,
                           state->songs[state->active_song],
                           state->selected_section,
                           state->selected_line))
    {
        application_state_notice(state, ONLYC_STRING_ERROR_OPEN_EDITOR);
        return;
    }
    controller_fill_artwork_color(controller);
    application_state_notify(state, change);
    if (request_artwork && controller->services.artwork)
    {
        controller->services.artwork(
            controller->services.context, state->artwork.url, state->artwork.request);
    }
}

int controller_commit_editor(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    if (!state->editing || !state->pending_changes)
    {
        return 1;
    }
    Song* next = malloc(sizeof(*next));
    if (!next)
    {
        application_state_notice(state, ONLYC_STRING_ERROR_SAVE);
        return 0;
    }
    memcpy(next, state->songs[state->active_song], sizeof(*next));
    int ok = 1;
    size_t next_line = state->selected_line;
    char error[256];
    snprintf(error, sizeof(error), "%s", ONLYC_STRING_ERROR_INPUT_BYTE_LIMIT);
#define COPY_METADATA(name, field)                                                                 \
    ok &= copy_field(&state->editor, field, next->name, sizeof(next->name))
    ok &= copy_field(
        &state->editor, EDITOR_TITLE, next->title.original, sizeof(next->title.original));
    ok &= copy_field(&state->editor,
                     EDITOR_TITLE_TRANSLATED,
                     next->title.translated,
                     sizeof(next->title.translated));
    ok &= copy_field(&state->editor,
                     EDITOR_TITLE_ANNOTATION,
                     next->title.annotation,
                     sizeof(next->title.annotation));
    COPY_METADATA(artist, EDITOR_ARTIST);
    COPY_METADATA(translator, EDITOR_TRANSLATOR);
    COPY_METADATA(slug, EDITOR_SLUG);
    COPY_METADATA(artwork, EDITOR_ARTWORK);
    COPY_METADATA(color, EDITOR_COLOR);
#undef COPY_METADATA
    next->link_count = state->editor.link_count;
    memset(next->links, 0, sizeof(next->links));
    for (size_t i = 0; i < state->editor.link_count; ++i)
    {
        const EditorLink* link = &state->editor.links[i];
        const char* label = link->label.data ? link->label.data : "";
        const char* url = link->url.data ? link->url.data : "";
        if (link->label.failed || link->url.failed ||
            strlen(label) >= sizeof(next->links[i].label) ||
            strlen(url) >= sizeof(next->links[i].url))
        {
            ok = 0;
            break;
        }
        strcpy(next->links[i].label, label);
        strcpy(next->links[i].url, url);
    }
    if (state->selected_section >= 0)
    {
        Section* section = &next->sections[state->selected_section];
        ok &= copy_field(&state->editor, EDITOR_LABEL, section->label, sizeof(section->label));
        ok &= copy_field(
            &state->editor, EDITOR_ORIGINAL, section->original, sizeof(section->original));
        ok &= copy_field(
            &state->editor, EDITOR_TRANSLATED, section->translated, sizeof(section->translated));
    }
    if (state->selected_section >= 0 || state->selected_section == TITLE_SECTION)
    {
        for (int field = EDITOR_ANNOTATION_REF; field < EDITOR_FIELD_COUNT; ++field)
        {
            const Buffer* value = &state->editor.fields[field];
            ok &= !value->failed && value->len < editor_field_capacity((EditorField)field);
        }
        if (ok)
        {
            ok = commit_annotation(controller, next, &next_line, error, sizeof(error));
        }
    }
    if (ok)
    {
        ok = song_validate(next, error, sizeof(error));
    }
    if (ok)
    {
        state->selected_line = next_line;
        memcpy(state->songs[state->active_song], next, sizeof(*next));
        if (state->selected_section >= 0 &&
            state->selected_line >
                lyrics_line_count(next->sections[state->selected_section].translated))
        {
            state->selected_line =
                lyrics_line_count(next->sections[state->selected_section].translated);
        }
        state->pending_changes = 0;
    }
    else
    {
        application_state_notice(state, error);
    }
    free(next);
    return ok;
}

void application_controller_import(ApplicationController* controller,
                                   const char* text,
                                   size_t length)
{
    ApplicationState* state = controller->state;
    state->import_in_progress = 0;
    Song* next = malloc(sizeof(*next));
    char error[256];
    if (!next)
    {
        application_state_notice(state, ONLYC_STRING_ERROR_IMPORT_MEMORY);
        return;
    }
    if (!song_parse(next, text, length, error, sizeof(error)))
    {
        application_state_notice(state, error);
        free(next);
        return;
    }
    if (state->song_dirty[(int)state->catalog.count] &&
        !controller->services.confirm(controller->services.context,
                                      ONLYC_STRING_CONFIRM_REPLACE_DRAFT))
    {
        free(next);
        return;
    }
    free(state->songs[(int)state->catalog.count]);
    state->songs[(int)state->catalog.count] = next;
    state->active_song = (int)state->catalog.count;
    controller_navigate(controller, "/#draft");
    state->editing = 1;
    state->selected_section = 0;
    state->selected_line = 1;
    state->pending_changes = 0;
    state->song_dirty[state->active_song] = 0;
    controller_refresh_dirty(controller);
    controller_present(controller, APPLICATION_RENDER);
    controller->view.focus(controller->view.context, VIEW_FOCUS_PANEL, 0, 0);
    application_state_notice(state, ONLYC_STRING_STATUS_IMPORTED);
}

void controller_export_song(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    Buffer yaml = {0};
    char name[100];
    if (song_export(state->songs[state->active_song], &yaml))
    {
        snprintf(name, sizeof(name), "%s.yaml", state->songs[state->active_song]->slug);
        if (controller->services.download(controller->services.context, name, &yaml))
        {
            state->song_dirty[state->active_song] = 0;
            controller_refresh_dirty(controller);
            application_state_notice(state, ONLYC_STRING_STATUS_DOWNLOADING);
        }
        else
        {
            application_state_notice(state, ONLYC_STRING_ERROR_DOWNLOAD);
        }
    }
    else
    {
        application_state_notice(state, ONLYC_STRING_ERROR_EXPORT);
    }
    buffer_free(&yaml);
}
