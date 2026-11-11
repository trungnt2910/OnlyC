#include "app/view/impl/ApplicationViewImpl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/ui/Dom.h"
#include "app/ui/Theme.h"
#include "platform/web/Browser.h"

static void handle_drop(BrowserApplicationView* view)
{
    if (!ui_event_has_files())
    {
        return;
    }
    ui_prevent_default();
    char filename[1024];
    if (ui_drop_name(filename, sizeof(filename)) < 0)
    {
        return;
    }
    if (application_controller_request_file(
            view->controller, filename, ui_drop_size(), ui_drop_count()))
    {
        ui_drop_read(MAX_IMPORT);
    }
}

void browser_view_event(BrowserApplicationView* view, int code)
{
    const ApplicationState* state = view->state;
    char value[80];
    switch (code)
    {
    case APP_EVENT_CLICK:
    {
        char index[32];
        if (ui_event_text("[data-action]", "data-action", value, sizeof(value)) < 0)
        {
            return;
        }
        if (ui_event_text("[data-action]", "data-index", index, sizeof(index)) < 0)
        {
            strcpy(index, "0");
        }
        int action = atoi(value);
        if ((action == APP_HOME || action == APP_SELECT_SONG || action == APP_RESUME_DRAFT) &&
            !ui_event_plain_click())
        {
            return;
        }
        ui_prevent_default();
        application_controller_action(view->controller, action, atoi(index));
        break;
    }
    case APP_EVENT_INPUT:
        if (ui_event_text("input,textarea", "id", value, sizeof(value)) < 0)
        {
            return;
        }
        if (!strcmp(value, "song-search"))
        {
            catalog_view_input(&view->catalog_impl);
        }
        else
        {
            editor_view_input(&view->editor_impl, value);
        }
        break;
    case APP_EVENT_KEY:
        reader_view_key(&view->reader_impl);
        break;
    case APP_EVENT_RESIZE:
        if (ui_width() > 760 && state->artwork.url[0])
        {
            ui_attribute("#song-artwork", "src", state->artwork.url);
        }
        theme_apply(view->state->artwork.color);
        break;
    case APP_EVENT_LOCATION:
        application_controller_location(view->controller);
        break;
    case APP_EVENT_UNLOAD:
        if (state->has_unexported_changes)
        {
            ui_prevent_default();
        }
        break;
    case APP_EVENT_DRAG:
        if (ui_event_has_files())
        {
            ui_prevent_default();
        }
        break;
    case APP_EVENT_DROP:
        handle_drop(view);
        break;
    case APP_EVENT_FILE:
        if (state->active_song != (int)state->catalog.count || state->import_in_progress)
        {
            return;
        }
        if (ui_event_text("input[type=file]", "id", value, sizeof(value)) < 0)
        {
            return;
        }
        {
            int size = ui_file_size("#import-file");
            if (size < 0)
            {
                return;
            }
            if (!application_controller_request_file(view->controller, "import.yaml", size, 1))
            {
                return;
            }
            ui_file_read("#import-file", MAX_IMPORT);
        }
        break;
    case APP_EVENT_INTERACTION:
        theme_interaction(view->state->artwork.color);
        break;
    }
}
