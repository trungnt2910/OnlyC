#include "app/controller/ControllerInternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/LyricLink.h"
#include "strings/Resources.h"

int controller_load_published_song(ApplicationController* controller, int index)
{
    ApplicationState* state = controller->state;
    if (state->songs[index])
    {
        return 1;
    }
    Song* song = malloc(sizeof(*song));
    char error[256];
    if (!song)
    {
        application_state_notice(state, ONLYC_STRING_ERROR_OPEN_SONG);
        return 0;
    }
    if (!song_parse(song,
                    state->catalog.sources[index],
                    strlen(state->catalog.sources[index]),
                    error,
                    sizeof(error)))
    {
        free(song);
        application_state_notice(state, error);
        return 0;
    }
    for (int i = 0; i < (int)state->catalog.count; ++i)
    {
        free(state->songs[i]);
        state->songs[i] = NULL;
    }
    state->songs[index] = song;
    return 1;
}

void application_controller_location(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    char path[sizeof(state->location)] = "";
    int length = controller->services.location(controller->services.context, path, sizeof(path));
    if (length < 0 || (size_t)length >= sizeof(path))
    {
        return; /* Do not act on a truncated pathname or fragment. */
    }
    if (state->location[0] && !strcmp(state->location, path))
    {
        return; /* Hash traversal may dispatch both popstate and hashchange. */
    }
    if (!controller_commit_editor(controller))
    {
        return;
    }
    strcpy(state->location, path);
    char* fragment = strchr(path, '#');
    state->active_song = PAGE_NOT_FOUND;
    state->selected_section = -1;
    state->editing = 0;
    if (!strcmp(path, "/#draft") && state->songs[(int)state->catalog.count])
    {
        state->active_song = (int)state->catalog.count;
        state->editing = 1;
    }
    else
    {
        if (fragment)
        {
            *fragment = 0;
        }
        if (!strcmp(path, "/") || !strcmp(path, "/index.html") || !path[0])
        {
            state->active_song = PAGE_HOME;
        }
    }
    for (int i = 0; i < (int)state->catalog.count; ++i)
    {
        char route[192];
        snprintf(route, sizeof(route), "/s/%s/", state->catalog.slugs[i]);
        if (!strcmp(path, route))
        {
            state->active_song = i;
        }
        route[strlen(route) - 1] = 0;
        if (!strcmp(path, route))
        {
            state->active_song = i;
        }
        snprintf(route, sizeof(route), "/%s.html", state->catalog.slugs[i]);
        if (!strcmp(path, route))
        {
            state->active_song = i;
        }
        snprintf(route, sizeof(route), "/s/%s/index.html", state->catalog.slugs[i]);
        if (!strcmp(path, route))
        {
            state->active_song = i;
        }
    }
    if (state->active_song >= 0 && state->active_song < (int)state->catalog.count &&
        !controller_load_published_song(controller, state->active_song))
    {
        state->active_song = PAGE_NOT_FOUND;
    }
    LyricTarget target;
    int selected =
        state->active_song >= 0 && state->active_song < (int)state->catalog.count && fragment &&
        lyric_link_resolve(
            state->songs[state->active_song], state->location + (fragment - path), &target);
    if (selected)
    {
        state->selected_section = target.section;
        state->selected_line = target.line;
    }
    controller_present(controller, APPLICATION_RENDER);
    if (selected)
    {
        controller->view.focus(controller->view.context,
                               target.section == TITLE_SECTION ? VIEW_FOCUS_TITLE_AND_SCROLL
                                                               : VIEW_FOCUS_LINE_AND_SCROLL,
                               target.section == TITLE_SECTION ? 0 : (size_t)target.section,
                               target.line);
        controller->view.focus(controller->view.context, VIEW_FOCUS_PANEL, 0, 0);
    }
}

void controller_navigate(ApplicationController* controller, const char* path)
{
    ApplicationState* state = controller->state;
    if (strcmp(state->location, path))
    {
        snprintf(state->location, sizeof(state->location), "%s", path);
        controller->services.navigate(controller->services.context, path);
    }
}

void controller_selection_url(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    if (state->active_song < 0 || state->active_song >= (int)state->catalog.count)
    {
        return;
    }
    const Song* song = state->songs[state->active_song];
    char fragment[40], path[sizeof(state->location)], base[sizeof(state->location)];
    if (!lyric_link_format(
            song, state->selected_section, state->selected_line, fragment, sizeof(fragment)))
    {
        return;
    }
    snprintf(base, sizeof(base), "%s", state->location);
    if (!base[0])
    {
        snprintf(base, sizeof(base), "/s/%s/", song->slug);
    }
    char* hash = strchr(base, '#');
    if (hash)
    {
        *hash = 0;
    }
    int length = snprintf(path, sizeof(path), "%s%s", base, fragment);
    if (length >= 0 && (size_t)length < sizeof(path))
    {
        controller_navigate(controller, path);
    }
}
