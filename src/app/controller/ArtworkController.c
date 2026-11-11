#include "app/controller/ControllerInternal.h"

#include <stdio.h>
#include <string.h>

#include "core/Color.h"

int controller_prepare_artwork(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    const Song* song = state->active_song < 0 ? NULL : state->songs[state->active_song];
    const char* url = song ? song->artwork : "";
    const char* seed = song ? song->color : "";
    if (!strcmp(url, state->artwork.url) && !strcmp(seed, state->artwork.seed))
    {
        return 0;
    }
    snprintf(state->artwork.url, sizeof(state->artwork.url), "%s", url);
    snprintf(state->artwork.seed, sizeof(state->artwork.seed), "%s", seed);
    Color parsed;
    state->artwork.color[0] = 0;
    if (color_parse(seed, &parsed))
    {
        color_format(parsed, state->artwork.color);
    }
    ++state->artwork.request;
    state->artwork.complete = 0;
    return 1;
}

void application_controller_artwork_ready(ApplicationController* controller,
                                          unsigned request,
                                          const char* color)
{
    ApplicationState* state = controller->state;
    Color parsed;
    if (request != state->artwork.request || !state->artwork.url[0] || state->artwork.complete)
    {
        return;
    }
    state->artwork.complete = 1;
    if (!color_parse(color, &parsed))
    {
        return; /* Failed downloads/CORS leave the authored color intact. */
    }
    char normalized[8];
    color_format(parsed, normalized);
    if (!color_similar(normalized, state->artwork.color))
    {
        strcpy(state->artwork.color, normalized);
        application_state_notify(state, APPLICATION_THEME_CHANGED);
    }
    controller_fill_artwork_color(controller);
}

void controller_fill_artwork_color(ApplicationController* controller)
{
    ApplicationState* state = controller->state;
    if (!state->artwork.complete || !state->artwork.color[0])
    {
        return;
    }
    /* Populate derived metadata for authors, without overwriting an in-flight
     * manual field edit or modifying the last validated song. */
    const Song* song = state->active_song < 0 ? NULL : state->songs[state->active_song];
    if (song && state->editing &&
        !strcmp(editor_state_text(&state->editor, EDITOR_ARTWORK), state->artwork.url) &&
        !strcmp(editor_state_text(&state->editor, EDITOR_COLOR), song->color) &&
        strcmp(editor_state_text(&state->editor, EDITOR_COLOR), state->artwork.color) &&
        !state->editor.fields[EDITOR_COLOR].failed)
    {
        application_controller_changed(controller, EDITOR_COLOR, state->artwork.color);
    }
}
