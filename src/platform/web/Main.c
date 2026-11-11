#include <stdlib.h>

#include "app/controller/ApplicationController.h"
#include "app/view/impl/ApplicationViewImpl.h"
#include "embedded.h"
#include "interop/Bridge.h"
#include "platform/web/Browser.h"
#include "platform/web/Services.h"

typedef struct
{
    ApplicationState state;
    ApplicationController controller;
    BrowserApplicationView view;
} Application;

/* Composition root; callbacks borrow these objects for the application's lifetime. */
static Application application;

static void on_event(void* context, int code)
{
    Application* current = context;
    browser_view_event(&current->view, code);
}

static void on_file(void* context, const char* text, size_t length)
{
    Application* current = context;
    application_controller_import(&current->controller, text, length);
}

static void on_file_error(void* context)
{
    Application* current = context;
    application_controller_file_error(&current->controller);
}

static void on_artwork(void* context, unsigned request, const char* color)
{
    Application* current = context;
    application_controller_artwork_ready(&current->controller, request, color);
}

EXPORT("app_start") void app_start(void)
{
    emlite_init_handle_table();
    SongCatalog catalog = {.count = SONG_COUNT,
                           .sources = embedded_songs,
                           .titles = embedded_titles,
                           .slugs = embedded_slugs};
    if (!application_state_init(&application.state, catalog))
    {
        abort();
    }
    application_controller_init(&application.controller,
                                &application.state,
                                browser_view_port(&application.view),
                                browser_services());
    browser_view_attach(&application.view, &application.controller);
    browser_set_callbacks((BrowserCallbacks){.context = &application,
                                             .event = on_event,
                                             .file = on_file,
                                             .file_error = on_file_error,
                                             .artwork = on_artwork});
    application_controller_start(&application.controller);
    browser_view_listen();
    ui_attribute("html", "data-ready", "true");
}
