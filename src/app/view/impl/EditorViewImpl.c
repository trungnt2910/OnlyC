#include "app/view/impl/EditorViewImpl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ui/Theme.h"
#include "app/view/impl/Controls.h"
#include "strings/Resources.h"

static const char* const field_names[EDITOR_FIELD_COUNT] = {
    [EDITOR_TITLE] = "title",
    [EDITOR_TITLE_TRANSLATED] = "title-translated",
    [EDITOR_TITLE_ANNOTATION] = "title-annotation",
    [EDITOR_ARTIST] = "artist",
    [EDITOR_TRANSLATOR] = "translator",
    [EDITOR_SLUG] = "slug",
    [EDITOR_ARTWORK] = "artwork",
    [EDITOR_COLOR] = "color",
    [EDITOR_LABEL] = "label",
    [EDITOR_ORIGINAL] = "original",
    [EDITOR_TRANSLATED] = "translated",
    [EDITOR_ANNOTATION_REF] = "annotation-ref",
    [EDITOR_ANNOTATION] = "annotation",
    [EDITOR_ANNOTATION_START] = "annotation-start",
    [EDITOR_ANNOTATION_END] = "annotation-end"};

static Node
field(EditorViewImpl* view, Node parent, EditorField name, const char* label, int multiline)
{
    Node input =
        view_field(parent,
                   field_names[name],
                   label,
                   editor_state_text(&view->state->editor, name),
                   multiline,
                   name >= EDITOR_ANNOTATION_START ? 3 : (int)editor_field_capacity(name) - 1);
    view->fields[name] = input;
    return input;
}

static void number_field(EditorViewImpl* view, Node parent, EditorField name, const char* label)
{
    Node input = field(view, parent, name, label, 0);
    dom_attr(input, "type", "number");
    dom_attr(input, "min", "1");
    dom_attr(input, "step", "1");
    attribute_number(input, "max", MAX_LINES);
}

static void mount_metadata(void* context, Node page)
{
    EditorViewImpl* view = context;
    Node editor = element(page, "section", "editor-meta", NULL);
    dom_attr(editor, "aria-label", ONLYC_STRING_EDITOR_METADATA_LABEL);
    Node toolbar = element(editor, "div", "editor-toolbar", NULL);
    element(toolbar, "p", NULL, ONLYC_STRING_EDITOR_SAVE_HINT);
    Node buttons = element(toolbar, "div", "toolbar-buttons", NULL);
    view_button(buttons, APP_IMPORT, 0, "button secondary", ONLYC_STRING_EDITOR_IMPORT);
    view_button(buttons, APP_EXPORT, 0, "button primary", ONLYC_STRING_EDITOR_EXPORT);
    Node details = element(editor, "details", NULL, NULL);
    if (view->state->pending_changes)
    {
        dom_attr(details, "open", "");
    }
    element(details, "summary", NULL, ONLYC_STRING_EDITOR_DETAILS);
    Node fields = element(details, "div", "metadata-fields", NULL);
    field(view, fields, EDITOR_TITLE, ONLYC_STRING_EDITOR_TITLE, 0);
    field(view, fields, EDITOR_TITLE_TRANSLATED, ONLYC_STRING_EDITOR_TITLE_TRANSLATED, 0);
    field(view, fields, EDITOR_TITLE_ANNOTATION, ONLYC_STRING_EDITOR_TITLE_ANNOTATION, 0);
    field(view, fields, EDITOR_ARTIST, ONLYC_STRING_EDITOR_ARTIST, 0);
    field(view, fields, EDITOR_TRANSLATOR, ONLYC_STRING_EDITOR_TRANSLATOR, 0);
    field(view, fields, EDITOR_SLUG, ONLYC_STRING_EDITOR_SLUG, 0);
    field(view, fields, EDITOR_ARTWORK, ONLYC_STRING_EDITOR_ARTWORK, 0);
    field(view, fields, EDITOR_COLOR, ONLYC_STRING_EDITOR_COLOR, 0);
    Node links = element(details, "div", "link-editor", NULL);
    for (size_t i = 0; i < view->state->editor.link_count; ++i)
    {
        const EditorLink* link = &view->state->editor.links[i];
        Node row = element(links, "div", "link-fields", NULL);
        char id[48];
        snprintf(id, sizeof(id), "link-label-%zu", i);
        view->link_labels[i] = view_field(row,
                                          id,
                                          ONLYC_STRING_EDITOR_LINK_LABEL,
                                          link->label.data ? link->label.data : "",
                                          0,
                                          sizeof(((SongLink*)0)->label) - 1);
        snprintf(id, sizeof(id), "link-url-%zu", i);
        view->link_urls[i] = view_field(row,
                                        id,
                                        ONLYC_STRING_EDITOR_LINK_URL,
                                        link->url.data ? link->url.data : "",
                                        0,
                                        sizeof(((SongLink*)0)->url) - 1);
        view_button(row, APP_REMOVE_LINK, (int)i, "button", ONLYC_STRING_EDITOR_REMOVE_LINK);
    }
    view_button(links, APP_ADD_LINK, 0, "button", ONLYC_STRING_EDITOR_ADD_LINK);
}

static void mount_annotation(void* context, Node panel)
{
    EditorViewImpl* view = context;
    const ApplicationState* state = view->state;
    const Song* song = state->songs[state->active_song];
    int annotation = annotation_for_selection(song, state->selected_section, state->selected_line);
    if (state->selected_section >= 0)
    {
        field(view, panel, EDITOR_LABEL, ONLYC_STRING_EDITOR_SECTION_LABEL, 0);
        Node lyrics = element(panel, "details", "section-editor", NULL);
        element(lyrics, "summary", NULL, ONLYC_STRING_EDITOR_SECTION_LYRICS);
        field(view, lyrics, EDITOR_ORIGINAL, ONLYC_STRING_EDITOR_ORIGINAL, 1);
        field(view, lyrics, EDITOR_TRANSLATED, ONLYC_STRING_EDITOR_TRANSLATED, 1);
        Node range = element(panel, "div", "annotation-range", NULL);
        number_field(view, range, EDITOR_ANNOTATION_START, ONLYC_STRING_EDITOR_RANGE_START);
        number_field(view, range, EDITOR_ANNOTATION_END, ONLYC_STRING_EDITOR_RANGE_END);
    }
    Node key = field(view, panel, EDITOR_ANNOTATION_REF, ONLYC_STRING_EDITOR_ANNOTATION_REF, 0);
    dom_attr(key, "list", "annotation-keys");
    Node options = element(panel, "datalist", NULL, NULL);
    dom_attr(options, "id", "annotation-keys");
    for (size_t i = 0; i < song->annotation_count; ++i)
    {
        dom_attr(element(options, "option", NULL, NULL), "value", song->annotations[i].key);
    }
    field(view, panel, EDITOR_ANNOTATION, ONLYC_STRING_EDITOR_ANNOTATION, 1);
    element(panel, "span", "eyebrow preview-label", ONLYC_STRING_EDITOR_PREVIEW);
    view->preview = element(panel, "div", "markdown", NULL);
    dom_attr(view->preview, "id", "markdown-preview");
    dom_markdown(view->preview, editor_state_text(&state->editor, EDITOR_ANNOTATION));
    Node buttons = element(panel, "div", "panel-buttons", NULL);
    view_button(buttons, APP_APPLY, 0, "button primary", ONLYC_STRING_EDITOR_APPLY);
    if (annotation >= 0)
    {
        view_button(
            buttons, APP_REMOVE_ANNOTATION, 0, "button", ONLYC_STRING_EDITOR_REMOVE_ANNOTATION);
    }
    if (state->selected_section >= 0)
    {
        view_button(
            buttons, APP_DELETE_SECTION, 0, "button danger", ONLYC_STRING_EDITOR_DELETE_SECTION);
    }
}

static void unmount_annotation(void* context)
{
    EditorViewImpl* view = context;
    for (int field = EDITOR_LABEL; field < EDITOR_FIELD_COUNT; ++field)
    {
        view->fields[field] = 0;
    }
    view->preview = 0;
}

static void unmount(void* context)
{
    EditorViewImpl* view = context;
    memset(view->fields, 0, sizeof(view->fields));
    view->preview = 0;
    memset(view->link_labels, 0, sizeof(view->link_labels));
    memset(view->link_urls, 0, sizeof(view->link_urls));
}

static void on_state_changed(void* context, ApplicationChange change)
{
    EditorViewImpl* view = context;
    if (change == APPLICATION_LINK_CHANGED)
    {
        const EditorState* editor = &view->state->editor;
        size_t index = editor->changed_link;
        if (index >= editor->link_count)
        {
            return;
        }
        int url = editor->changed_link_url;
        Node node = url ? view->link_urls[index] : view->link_labels[index];
        if (!node)
        {
            return;
        }
        const Buffer* value = url ? &editor->links[index].url : &editor->links[index].label;
        const char* text = value->data ? value->data : "";
        char id[48], current[4 * sizeof(((SongLink*)0)->url)];
        snprintf(id, sizeof(id), "link-%s-%zu", url ? "url" : "label", index);
        if (ui_read(id, current, sizeof(current)) < 0 || strcmp(current, text))
        {
            dom_value(node, text);
        }
        return;
    }
    EditorField field = view->state->editor.changed_field;
    if (change != APPLICATION_EDITOR_CHANGED || field < 0 || field >= EDITOR_FIELD_COUNT ||
        !view->fields[field])
    {
        return;
    }
    const char* text = editor_state_text(&view->state->editor, field);
    char current[4 * sizeof(((Annotation*)0)->text)];
    /* Keep the focused element and caret when its input already matches state. */
    if (ui_read(field_names[field], current, sizeof(current)) < 0 || strcmp(current, text))
    {
        dom_value(view->fields[field], text);
    }
    if (field == EDITOR_ANNOTATION && view->preview)
    {
        dom_clear(view->preview);
        dom_markdown(view->preview, text);
        theme_apply(view->state->artwork.color);
    }
}

void editor_view_init(EditorViewImpl* view, ApplicationController* controller)
{
    view->state = controller->state;
    view->controller = controller;
    application_state_subscribe(controller->state, &view->subscription, on_state_changed, view);
}

void editor_view_destroy(EditorViewImpl* view)
{
    application_state_unsubscribe(&view->subscription);
    unmount(view);
    view->controller = NULL;
    view->state = NULL;
}

EditorView editor_view_port(EditorViewImpl* view)
{
    return (EditorView){.context = view,
                        .mount_metadata = mount_metadata,
                        .mount_annotation = mount_annotation,
                        .unmount_annotation = unmount_annotation,
                        .unmount = unmount};
}

void editor_view_input(EditorViewImpl* view, const char* id)
{
    for (size_t index = 0; index < view->state->editor.link_count; ++index)
    {
        for (int url = 0; url <= 1; ++url)
        {
            char expected[48];
            snprintf(expected, sizeof(expected), "link-%s-%zu", url ? "url" : "label", index);
            if (!strcmp(id, expected))
            {
                char text[4 * sizeof(((SongLink*)0)->url)];
                int read = ui_read(id, text, sizeof(text));
                application_controller_link_changed(
                    view->controller, index, url, read < 0 ? NULL : text);
                return;
            }
        }
    }
    for (int field = 0; field < EDITOR_FIELD_COUNT; ++field)
    {
        if (view->fields[field] && !strcmp(id, field_names[field]))
        {
            /* HTML maxlength counts UTF-16 units; preserve all supported input
             * before the controller enforces the song's UTF-8 byte limit. */
            size_t capacity = 4 * editor_field_capacity((EditorField)field);
            char* text = malloc(capacity);
            int read = text ? ui_read(id, text, (int)capacity) : -1;
            application_controller_changed(
                view->controller, (EditorField)field, read < 0 ? NULL : text);
            free(text);
            return;
        }
    }
}
