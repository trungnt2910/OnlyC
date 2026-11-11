#include "app/controller/ApplicationController.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/controller/ControllerInternal.h"
#include "strings/Resources.h"

void application_controller_action(ApplicationController* controller, int action, int index)
{
    ApplicationState* state = controller->state;
    if (action == APP_IMPORT)
    {
        if (state->active_song != (int)state->catalog.count)
        {
            return;
        }
        controller->services.select_file(controller->services.context);
        return;
    }
    if (action == APP_ADD_LINK || action == APP_REMOVE_LINK)
    {
        if (!state->editing)
        {
            return;
        }
        if (action == APP_ADD_LINK)
        {
            if (state->editor.link_count == MAX_LINKS)
            {
                application_state_notice(state, ONLYC_STRING_ERROR_MAX_LINKS);
                return;
            }
            ++state->editor.link_count;
        }
        else
        {
            if (index < 0 || (size_t)index >= state->editor.link_count)
            {
                return;
            }
            editor_link_remove(&state->editor, (size_t)index);
        }
        state->pending_changes = 1;
        state->song_dirty[state->active_song] = 1;
        controller_refresh_dirty(controller);
        controller_present(controller, APPLICATION_RENDER);
        return;
    }
    int had_pending_changes = state->pending_changes;
    if (!controller_commit_editor(controller))
    {
        return;
    }
    int focus_panel = 0;
    int restore_focus = 0;
    size_t restore_section = 0, restore_line = 1;
    switch (action)
    {
    case APP_SELECT_SONG:
        if (index < 0 || index >= (int)state->catalog.count)
        {
            return;
        }
        if (!controller_load_published_song(controller, index))
        {
            return;
        }
        state->active_song = index;
        state->editing = 0;
        state->selected_section = -1;
        application_state_notice(state, "");
        {
            char route[192];
            snprintf(route, sizeof(route), "/s/%s/", state->catalog.slugs[state->active_song]);
            controller_navigate(controller, route);
        }
        break;
    case APP_HOME:
        state->active_song = PAGE_HOME;
        state->selected_section = -1;
        state->editing = 0;
        controller_navigate(controller, "/");
        application_state_notice(state, "");
        break;
    case APP_RESUME_DRAFT:
        if (!state->songs[(int)state->catalog.count])
        {
            return;
        }
        state->active_song = (int)state->catalog.count;
        state->selected_section = -1;
        state->editing = 1;
        controller_navigate(controller, "/#draft");
        break;
    case APP_TOGGLE_EDIT:
        if (state->active_song != (int)state->catalog.count)
        {
            return;
        }
        state->editing = !state->editing;
        break;
    case APP_CLOSE_PANEL:
        if (state->selected_section == TITLE_SECTION)
        {
            restore_focus = 2;
        }
        if (state->selected_section >= 0)
        {
            restore_focus = 1;
            restore_section = (size_t)state->selected_section;
            restore_line = state->selected_line;
        }
        state->selected_section = -1;
        break;
    case APP_APPLY:
        if (!state->editing)
        {
            return;
        }
        application_state_notice(state, ONLYC_STRING_STATUS_APPLIED);
        break;
    case APP_EXPORT:
        if (state->active_song != (int)state->catalog.count)
        {
            return;
        }
        controller_export_song(controller);
        break;
    case APP_SELECT_TITLE:
        if (state->active_song < 0)
        {
            return;
        }
        state->selected_section = TITLE_SECTION;
        state->selected_line = 1;
        focus_panel = 1;
        break;
    case APP_SELECT_LINE:
    {
        if (index < 0 || state->active_song < 0)
        {
            return;
        }
        size_t section = (size_t)index / MAX_LINES;
        size_t line = (size_t)index % MAX_LINES + 1;
        if (section >= state->songs[state->active_song]->count ||
            line >
                lyrics_line_count(state->songs[state->active_song]->sections[section].translated))
        {
            return;
        }
        state->selected_section = (int)section;
        state->selected_line = line;
        focus_panel = 1;
        break;
    }
    case APP_ADD_SECTION:
        if (!state->editing)
        {
            return;
        }
        if (state->songs[state->active_song]->count == MAX_SECTIONS)
        {
            application_state_notice(state, ONLYC_STRING_ERROR_MAX_SECTIONS);
            return;
        }
        state->selected_section = (int)state->songs[state->active_song]->count++;
        state->selected_line = 1;
        memset(&state->songs[state->active_song]->sections[state->selected_section],
               0,
               sizeof(Section));
        snprintf(state->songs[state->active_song]->sections[state->selected_section].label,
                 sizeof(state->songs[state->active_song]->sections[state->selected_section].label),
                 "%s",
                 ONLYC_STRING_EDITOR_NEW_SECTION);
        state->song_dirty[state->active_song] = 1;
        controller_refresh_dirty(controller);
        focus_panel = 1;
        break;
    case APP_DELETE_SECTION:
        if (!state->editing || state->selected_section < 0)
        {
            return;
        }
        if (state->songs[state->active_song]->count == 1)
        {
            application_state_notice(state, ONLYC_STRING_ERROR_KEEP_SECTION);
            return;
        }
        if (!controller->services.confirm(controller->services.context,
                                          ONLYC_STRING_CONFIRM_DELETE_SECTION))
        {
            return;
        }
        song_remove_section(state->songs[state->active_song], (size_t)state->selected_section);
        state->selected_section = -1;
        state->song_dirty[state->active_song] = 1;
        controller_refresh_dirty(controller);
        break;
    case APP_REMOVE_ANNOTATION:
    {
        if (!state->editing || state->selected_section == -1)
        {
            return;
        }
        Song* song = state->songs[state->active_song];
        if (state->selected_section == TITLE_SECTION)
        {
            song->title.annotation[0] = 0;
        }
        else
        {
            int range = annotation_range_for_line(
                song, (size_t)state->selected_section, state->selected_line);
            if (range >= 0)
            {
                song_remove_range(song, (size_t)range);
            }
        }
        state->song_dirty[state->active_song] = 1;
        controller_refresh_dirty(controller);
        focus_panel = 1;
        break;
    }
    default:
        return;
    }
    if (action == APP_SELECT_LINE || action == APP_SELECT_TITLE || action == APP_CLOSE_PANEL)
    {
        controller_selection_url(controller);
    }
    ApplicationChange change =
        !had_pending_changes && (action == APP_SELECT_LINE || action == APP_SELECT_TITLE ||
                                 action == APP_CLOSE_PANEL)
            ? APPLICATION_READER_CHANGED
            : APPLICATION_RENDER;
    controller_present(controller, change);
    if (focus_panel)
    {
        controller->view.focus(controller->view.context, VIEW_FOCUS_PANEL, 0, 0);
    }
    if (restore_focus)
    {
        controller->view.focus(controller->view.context,
                               restore_focus == 2 ? VIEW_FOCUS_TITLE : VIEW_FOCUS_LINE,
                               restore_section,
                               restore_line);
    }
}

void application_controller_init(ApplicationController* controller,
                                 ApplicationState* state,
                                 ApplicationView view,
                                 ApplicationServices services)
{
    *controller = (ApplicationController){.state = state, .view = view, .services = services};
}

void application_controller_start(ApplicationController* controller)
{
    application_state_reset(controller->state);
    application_controller_location(controller);
}

void application_controller_changed(ApplicationController* controller,
                                    EditorField field,
                                    const char* value)
{
    ApplicationState* state = controller->state;
    if (!state->editing || field < 0 || field >= EDITOR_FIELD_COUNT ||
        (field >= EDITOR_LABEL && state->selected_section < 0 &&
         !(state->selected_section == TITLE_SECTION && field >= EDITOR_ANNOTATION_REF)))
    {
        return;
    }
    int stored = value && editor_state_set(&state->editor, field, value);
    if (!stored)
    {
        state->editor.fields[field].failed = 1;
    }
    if (stored && (field == EDITOR_ANNOTATION_REF ||
                   (field == EDITOR_TITLE_ANNOTATION && state->selected_section == TITLE_SECTION)))
    {
        const Song* song = state->songs[state->active_song];
        int note = annotation_by_key(song, value);
        if (state->selected_section == TITLE_SECTION)
        {
            EditorField other =
                field == EDITOR_ANNOTATION_REF ? EDITOR_TITLE_ANNOTATION : EDITOR_ANNOTATION_REF;
            if (!editor_state_set(&state->editor, other, value))
            {
                state->editor.fields[other].failed = 1;
            }
            application_state_notify(state, APPLICATION_EDITOR_CHANGED);
        }
        if (note >= 0)
        {
            if (!editor_state_set(&state->editor, EDITOR_ANNOTATION, song->annotations[note].text))
            {
                state->editor.fields[EDITOR_ANNOTATION].failed = 1;
            }
            application_state_notify(state, APPLICATION_EDITOR_CHANGED);
        }
        state->editor.changed_field = field;
    }
    state->pending_changes = 1;
    state->song_dirty[state->active_song] = 1;
    controller_refresh_dirty(controller);
    if (stored)
    {
        application_state_notify(state, APPLICATION_EDITOR_CHANGED);
    }
    if (stored)
    {
        application_state_notice(state, ONLYC_STRING_STATUS_UNEXPORTED);
    }
    else
    {
        application_state_notice(state, ONLYC_STRING_ERROR_KEEP_EDIT);
    }
}

void application_controller_search(ApplicationController* controller, const char* query)
{
    ApplicationState* state = controller->state;
    if (state->active_song != PAGE_HOME)
    {
        return;
    }
    snprintf(state->search_query, sizeof(state->search_query), "%s", query);
    application_state_notify(state, APPLICATION_INDEX_CHANGED);
}

void application_controller_file_error(ApplicationController* controller)
{
    controller->state->import_in_progress = 0;
    application_state_notice(controller->state, ONLYC_STRING_ERROR_READ_FILE);
}

void application_controller_link_changed(ApplicationController* controller,
                                         size_t index,
                                         int url,
                                         const char* value)
{
    ApplicationState* state = controller->state;
    if (!state->editing || index >= state->editor.link_count)
    {
        return;
    }
    int stored = editor_link_set(&state->editor, index, url, value);
    if (stored)
    {
        state->editor.changed_link = index;
        state->editor.changed_link_url = url;
        application_state_notify(state, APPLICATION_LINK_CHANGED);
    }
    state->pending_changes = 1;
    state->song_dirty[state->active_song] = 1;
    controller_refresh_dirty(controller);
    if (stored)
    {
        application_state_notice(state, ONLYC_STRING_STATUS_UNEXPORTED);
    }
    else
    {
        application_state_notice(state, ONLYC_STRING_ERROR_KEEP_EDIT);
    }
}
