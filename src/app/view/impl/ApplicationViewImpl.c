#include "app/view/impl/ApplicationViewImpl.h"

#include <stdio.h>
#include <string.h>

#include "app/ui/Theme.h"
#include "app/view/impl/Controls.h"
#include "platform/web/Browser.h"
#include "strings/Resources.h"

static void render_header(const ApplicationState* state, Node root)
{
    Node header = element(root, "header", "topbar", NULL);
    Node brand = view_route_link(header, "brand", ONLYC_STRING_BRAND_NAME, "/", APP_HOME, 0);
    dom_attr(brand, "aria-label", ONLYC_STRING_SITE_NAME " " ONLYC_STRING_HOME);
    element(brand, "span", NULL, ONLYC_STRING_BRAND_SUFFIX);
    if (state->active_song == (int)state->catalog.count)
    {
        view_button(header,
                    APP_TOGGLE_EDIT,
                    0,
                    "mode",
                    state->editing ? ONLYC_STRING_EDITOR_PREVIEW : ONLYC_STRING_EDITOR_EDIT_DRAFT);
    }
    else if (state->songs[(int)state->catalog.count])
    {
        view_route_link(header,
                        "resume-draft",
                        ONLYC_STRING_EDITOR_RESUME_DRAFT,
                        "/#draft",
                        APP_RESUME_DRAFT,
                        0);
    }
}

static void render(BrowserApplicationView* view)
{
    const ApplicationState* state = view->state;
    view->catalog.unmount(view->catalog.context);
    view->reader.unmount(view->reader.context);
    Node root = dom_reset("#app");
    render_header(state, root);
    Node page = element(root, "div", "page", NULL);
    if (state->active_song == PAGE_HOME)
    {
        view->catalog.mount(view->catalog.context, page);
        ui_title(ONLYC_STRING_CATALOG_TITLE ONLYC_STRING_TITLE_SEPARATOR ONLYC_STRING_SITE_NAME);
    }
    else if (state->active_song == PAGE_NOT_FOUND)
    {
        element(page, "h1", "index-heading", ONLYC_STRING_READER_NOT_FOUND);
        view_route_link(page, "back-link", ONLYC_STRING_READER_BACK, "/", APP_HOME, 0);
        ui_title(ONLYC_STRING_READER_NOT_FOUND ONLYC_STRING_TITLE_SEPARATOR ONLYC_STRING_SITE_NAME);
    }
    else
    {
        const Song* song = state->songs[state->active_song];
        view_route_link(page, "back-link", ONLYC_STRING_READER_BACK, "/", APP_HOME, 0);
        view->reader.mount(view->reader.context, page);
        if (state->active_song == (int)state->catalog.count)
        {
            Node file = element(root, "input", NULL, NULL);
            dom_attr(file, "type", "file");
            dom_attr(file, "id", "import-file");
            dom_attr(file, "hidden", "");
            dom_attr(file, "accept", ".yaml,.yml,text/yaml,application/yaml");
        }
        char title[sizeof(song->title.original) + sizeof(ONLYC_STRING_TITLE_SEPARATOR) +
                   sizeof(ONLYC_STRING_SITE_NAME)];
        snprintf(title,
                 sizeof(title),
                 "%s%s%s",
                 song_title(song),
                 ONLYC_STRING_TITLE_SEPARATOR,
                 ONLYC_STRING_SITE_NAME);
        ui_title(title);
    }
    theme_apply(view->state->artwork.color);
}

static void focus(void* context, ViewFocus target, size_t section, size_t line)
{
    (void)context;
    if (target == VIEW_FOCUS_PANEL)
    {
        ui_focus("panel");
    }
    else
    {
        char id[48];
        if (target == VIEW_FOCUS_TITLE || target == VIEW_FOCUS_TITLE_AND_SCROLL)
        {
            snprintf(id, sizeof(id), "title-line");
        }
        else
        {
            view_line_id(id, sizeof(id), section, line);
        }
        if (target == VIEW_FOCUS_TITLE_AND_SCROLL || target == VIEW_FOCUS_LINE_AND_SCROLL)
        {
            ui_reveal(id);
        }
        ui_focus(id);
    }
}

static void on_state_changed(void* context, ApplicationChange change)
{
    BrowserApplicationView* view = context;
    const ApplicationState* state = view->state;
    switch (change)
    {
    case APPLICATION_RENDER:
        render(view);
        break;
    case APPLICATION_THEME_CHANGED:
        theme_apply(state->artwork.color);
        break;
    case APPLICATION_NOTICE_CHANGED:
        ui_toast(state->notice);
        theme_apply(view->state->artwork.color);
        break;
    default:
        break;
    }
}

ApplicationView browser_view_port(BrowserApplicationView* view)
{
    return (ApplicationView){.context = view, .focus = focus};
}

void browser_view_attach(BrowserApplicationView* view, ApplicationController* controller)
{
    view->controller = controller;
    view->state = controller->state;
    catalog_view_init(&view->catalog_impl, controller);
    editor_view_init(&view->editor_impl, controller);
    reader_view_init(&view->reader_impl, controller, editor_view_port(&view->editor_impl));
    view->catalog = catalog_view_port(&view->catalog_impl);
    view->reader = reader_view_port(&view->reader_impl);
    application_state_subscribe(controller->state, &view->subscription, on_state_changed, view);
}

void browser_view_detach(BrowserApplicationView* view)
{
    application_state_unsubscribe(&view->subscription);
    catalog_view_destroy(&view->catalog_impl);
    reader_view_destroy(&view->reader_impl);
    editor_view_destroy(&view->editor_impl);
    view->controller = NULL;
    view->state = NULL;
}

void browser_view_listen(void)
{
    ui_listen("document", "dragover", APP_EVENT_DRAG);
    ui_listen("document", "drop", APP_EVENT_DROP);
    ui_listen("document", "click", APP_EVENT_CLICK);
    ui_listen("document", "input", APP_EVENT_INPUT);
    ui_listen("document", "keydown", APP_EVENT_KEY);
    ui_listen("window", "resize", APP_EVENT_RESIZE);
    ui_listen("window", "popstate", APP_EVENT_LOCATION);
    ui_listen("window", "hashchange", APP_EVENT_LOCATION);
    ui_listen("window", "beforeunload", APP_EVENT_UNLOAD);
    ui_listen("document", "change", APP_EVENT_FILE);
    ui_listen("document", "mouseover", APP_EVENT_INTERACTION);
    ui_listen("document", "mouseout", APP_EVENT_INTERACTION);
    ui_listen("document", "focusin", APP_EVENT_INTERACTION);
    ui_listen("document", "focusout", APP_EVENT_INTERACTION);
}
