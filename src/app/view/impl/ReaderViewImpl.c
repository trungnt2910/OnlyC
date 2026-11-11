#include "app/view/impl/ReaderViewImpl.h"

#include <stdio.h>
#include <string.h>

#include "app/ui/Theme.h"
#include "app/view/impl/Controls.h"
#include "strings/Resources.h"

static Node render_metadata(Node page, const Song* song)
{
    Node hero = element(page, "section", "hero", NULL);
    dom_attr(hero, "aria-labelledby", "song-title");
    if (song->artwork[0])
    {
        Node cover = element(hero, "img", "artwork", NULL);
        dom_attr(cover, "id", "song-artwork");
        dom_attr(cover, "crossorigin", "anonymous");
        if (ui_width() > 760)
        {
            dom_attr(cover, "src", song->artwork);
        }
        dom_attr(cover, "alt", song_title(song));
        dom_attr(cover, "width", "160");
        dom_attr(cover, "height", "160");
        dom_attr(cover, "decoding", "async");
    }
    Node heading = element(hero, "h1", NULL, NULL);
    dom_attr(heading, "id", "song-title");
    Node title = view_button(heading,
                             APP_SELECT_TITLE,
                             0,
                             song->title.annotation[0] ? "title-line annotated" : "title-line",
                             song_title(song));
    dom_attr(title, "id", "title-line");
    dom_attr(title, "aria-expanded", "false");
    element(hero, "p", "artist", song->artist);
    Node metadata = element(hero, "div", "metadata", NULL);
    Node credit = element(metadata, "span", NULL, ONLYC_STRING_READER_CREDIT);
    element(credit,
            "strong",
            NULL,
            song->translator[0] ? song->translator : ONLYC_STRING_READER_ANONYMOUS);
    for (size_t i = 0; i < song->link_count; ++i)
    {
        Node link = element(metadata, "a", "source-link", song->links[i].label);
        dom_attr(link, "href", song->links[i].url);
        dom_attr(link, "target", "_blank");
        dom_attr(link, "rel", "noopener noreferrer");
        dom_attr(
            element(link, "span", NULL, ONLYC_STRING_READER_EXTERNAL_MARK), "aria-hidden", "true");
    }
    return hero;
}

static void render_lyrics(const ApplicationState* state, Node grid, const Song* song)
{
    Node lyrics = element(grid, "section", "lyrics", NULL);
    dom_attr(lyrics, "aria-label", ONLYC_STRING_READER_TRANSLATED_LABEL);
    int selected_note =
        annotation_for_selection(song, state->selected_section, state->selected_line);
    for (size_t section_index = 0; section_index < song->count; ++section_index)
    {
        const Section* section = &song->sections[section_index];
        Node stanza = element(lyrics, "div", "stanza", NULL);
        char heading[88];
        snprintf(heading, sizeof(heading), "[%s]", section->label);
        element(stanza, "h3", "section-label", heading);
        for (size_t line = 1; line <= lyrics_line_count(section->translated); ++line)
        {
            int note = annotation_for_line(song, section_index, line);
            int is_selected =
                state->selected_section == (int)section_index &&
                (line == state->selected_line || (selected_note >= 0 && note == selected_note));
            const char* class_name = note < 0 ? "lyric-line" : "lyric-line annotated";
            if (is_selected)
            {
                class_name = note < 0 ? "lyric-line selected" : "lyric-line annotated selected";
            }
            Buffer text = {0};
            lyrics_line_range(section->translated, line, line, &text);
            Node row = element(stanza, "div", "lyric-row", NULL);
            Node link = view_button(row,
                                    APP_SELECT_LINE,
                                    view_line_index(section_index, line),
                                    class_name,
                                    text.len ? text.data : "\xc2\xa0");
            char id[48], accessible_name[128];
            view_line_id(id, sizeof(id), section_index, line);
            dom_attr(link, "id", id);
            dom_attr(link, "aria-expanded", is_selected ? "true" : "false");
            attribute_number(link, "data-line", (int)line);
            attribute_number(link, "data-section", (int)section_index);
            if (!text.len)
            {
                snprintf(accessible_name,
                         sizeof(accessible_name),
                         ONLYC_STRING_READER_EMPTY_LINE_FORMAT,
                         section->label,
                         line);
                dom_attr(link, "aria-label", accessible_name);
            }
            buffer_free(&text);
        }
    }
    if (state->editing)
    {
        view_button(lyrics, APP_ADD_SECTION, 0, "add-section", ONLYC_STRING_EDITOR_ADD_SECTION);
    }
}

static void render_original(Node panel, const ApplicationState* state, const Song* song)
{
    Node original = element(panel, "div", "original-block", NULL);
    if (state->selected_section == TITLE_SECTION)
    {
        element(original, "p", "original-line selected", song->title.original);
        return;
    }
    size_t section_index = (size_t)state->selected_section;
    const Section* section = &song->sections[section_index];
    int selected = annotation_for_line(song, section_index, state->selected_line);
    for (size_t line = 1; line <= lyrics_line_count(section->original); ++line)
    {
        int highlighted = selected < 0 ? line == state->selected_line
                                       : annotation_for_line(song, section_index, line) == selected;
        Buffer text = {0};
        lyrics_line_range(section->original, line, line, &text);
        Node row = element(original,
                           "p",
                           highlighted ? "original-line selected" : "original-line",
                           text.len ? text.data : "\xc2\xa0");
        attribute_number(row, "data-line", (int)line);
        if (highlighted)
        {
            dom_attr(row, "aria-current", "true");
        }
        buffer_free(&text);
    }
}

static void render_panel(ReaderViewImpl* view, Node parent, const Song* song)
{
    const ApplicationState* state = view->state;
    if (state->selected_section == -1)
    {
        return;
    }
    int title_selected = state->selected_section == TITLE_SECTION;
    const Section* section = title_selected ? NULL : &song->sections[state->selected_section];
    int annotation_index =
        annotation_for_selection(song, state->selected_section, state->selected_line);
    const Annotation* note = annotation_index < 0 ? NULL : &song->annotations[annotation_index];
    int range_index = title_selected ? -1
                                     : annotation_range_for_line(song,
                                                                 (size_t)state->selected_section,
                                                                 state->selected_line);
    const AnnotationRange* range = range_index < 0 ? NULL : &song->ranges[range_index];
    size_t first = range ? range->first_line : state->selected_line;
    size_t last = range ? range->last_line : state->selected_line;
    dom_attr(view_button(parent, APP_CLOSE_PANEL, 0, "sheet-backdrop", NULL),
             "aria-label",
             ONLYC_STRING_READER_CLOSE_LABEL);
    Node panel = element(parent, "aside", "annotation-panel", NULL);
    dom_attr(panel, "id", "panel");
    dom_attr(panel, "tabindex", "-1");
    dom_attr(panel, "aria-labelledby", "panel-title");
    element(panel, "div", "sheet-handle", NULL);
    Node top = element(panel, "div", "panel-top", NULL);
    char title[128];
    if (title_selected)
    {
        snprintf(title, sizeof(title), "%s", song_title(song));
    }
    else if (first == last)
    {
        snprintf(title, sizeof(title), "%s · %zu", section->label, first);
    }
    else
    {
        snprintf(title, sizeof(title), "%s · %zu–%zu", section->label, first, last);
    }
    dom_attr(element(top, "h2", NULL, title), "id", "panel-title");
    view_button(top, APP_CLOSE_PANEL, 0, "close", ONLYC_STRING_READER_CLOSE);
    render_original(panel, state, song);
    if (state->editing)
    {
        view->editor.mount_annotation(view->editor.context, panel);
    }
    else if (note)
    {
        dom_markdown(element(panel, "div", "markdown", NULL), note->text);
    }
}

static void render_selection(ReaderViewImpl* view)
{
    const ApplicationState* state = view->state;
    const Song* song = state->songs[state->active_song];
    view->editor.unmount_annotation(view->editor.context);
    dom_clear(view->grid);
    dom_clear(view->title_panel_host);
    dom_attr(view->grid,
             "class",
             state->selected_section >= 0 ? "reading-grid has-panel" : "reading-grid");
    ui_attribute("#title-line",
                 "aria-expanded",
                 state->selected_section == TITLE_SECTION ? "true" : "false");
    ui_attribute(
        "#title-line",
        "class",
        state->selected_section == TITLE_SECTION
            ? (song->title.annotation[0] ? "title-line annotated selected" : "title-line selected")
            : (song->title.annotation[0] ? "title-line annotated" : "title-line"));
    render_lyrics(state, view->grid, song);
    render_panel(
        view, state->selected_section == TITLE_SECTION ? view->title_panel_host : view->grid, song);
}

static void mount(void* context, Node page)
{
    ReaderViewImpl* view = context;
    const ApplicationState* state = view->state;
    const Song* song = state->songs[state->active_song];
    Node hero = render_metadata(page, song);
    view->title_panel_host = element(hero, "div", "title-panel-host", NULL);
    dom_attr(view->title_panel_host, "id", "title-panel-host");
    if (state->editing)
    {
        view->editor.mount_metadata(view->editor.context, page);
    }
    element(element(page, "div", "reading-header", NULL), "h2", NULL, ONLYC_STRING_READER_LYRICS);
    view->grid = element(page, "div", "reading-grid", NULL);
    render_selection(view);
    if (!state->editing)
    {
        char term[sizeof(song->slug) + 5], backlink[sizeof(ONLYC_STRING_GISCUS_SITE_ORIGIN) + 96];
        snprintf(term, sizeof(term), "song:%s", song->slug);
        snprintf(
            backlink, sizeof(backlink), "%s/s/%s/", ONLYC_STRING_GISCUS_SITE_ORIGIN, song->slug);
        ui_discussion(term, song_description(song), backlink);
    }
}

static void unmount(void* context)
{
    ReaderViewImpl* view = context;
    view->editor.unmount(view->editor.context);
    view->grid = 0;
    view->title_panel_host = 0;
    ui_discussion(NULL, NULL, NULL);
}

static void on_state_changed(void* context, ApplicationChange change)
{
    ReaderViewImpl* view = context;
    if (change == APPLICATION_READER_CHANGED && view->grid)
    {
        render_selection(view);
        theme_apply(view->state->artwork.color);
    }
}

void reader_view_init(ReaderViewImpl* view, ApplicationController* controller, EditorView editor)
{
    view->state = controller->state;
    view->controller = controller;
    view->editor = editor;
    application_state_subscribe(controller->state, &view->subscription, on_state_changed, view);
}

void reader_view_destroy(ReaderViewImpl* view)
{
    application_state_unsubscribe(&view->subscription);
    unmount(view);
    view->controller = NULL;
    view->state = NULL;
}

ReaderView reader_view_port(ReaderViewImpl* view)
{
    return (ReaderView){.context = view, .mount = mount, .unmount = unmount};
}

void reader_view_key(ReaderViewImpl* view)
{
    const ApplicationState* state = view->state;
    char key[32];
    if (ui_event_key(key, sizeof(key)) < 0)
    {
        return;
    }
    if (!strcmp(key, "Escape"))
    {
        application_controller_action(view->controller, APP_CLOSE_PANEL, 0);
        return;
    }
    if (strcmp(key, "Tab") || state->selected_section == -1 || ui_width() > 760)
    {
        return;
    }
    const char* focusable =
        "#panel :is(button,a[href],input,textarea,summary):not(details:not([open]) > :not(summary) "
        "*)";
    int count = ui_count(focusable);
    if (!count)
    {
        return;
    }
    int current = ui_active_index(focusable);
    if (ui_event_shift() && (ui_active_matches("#panel") || current == 0))
    {
        ui_prevent_default();
        ui_focus_at(focusable, count - 1);
    }
    else if (!ui_event_shift() && current == count - 1)
    {
        ui_prevent_default();
        ui_focus_at(focusable, 0);
    }
}
