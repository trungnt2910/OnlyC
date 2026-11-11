#include "app/view/impl/CatalogViewImpl.h"

#include <stdio.h>
#include <string.h>

#include "app/ui/Theme.h"
#include "app/view/impl/Controls.h"
#include "strings/Resources.h"

static void render_results(const ApplicationState* state, Node container)
{
    size_t count = 0;
    for (size_t i = 0; i < state->catalog.count; ++i)
    {
        int index = state->sorted_songs[i].index;
        count += catalog_title_matches(state->catalog.titles[index], state->search_query);
    }
    char total[32 + sizeof(ONLYC_STRING_CATALOG_SONG_SINGULAR) +
               sizeof(ONLYC_STRING_CATALOG_SONG_PLURAL)];
    snprintf(total,
             sizeof(total),
             ONLYC_STRING_CATALOG_COUNT_FORMAT,
             count,
             count == 1 ? ONLYC_STRING_CATALOG_SONG_SINGULAR : ONLYC_STRING_CATALOG_SONG_PLURAL);
    element(container, "p", "index-count", total);
    Node alphabet = element(container, "nav", "index-alphabet", NULL);
    dom_attr(alphabet, "aria-label", ONLYC_STRING_CATALOG_BROWSE_LABEL);
    Node groups = element(container, "div", "index-groups", NULL);
    const char* letters = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (const char* letter = letters; *letter; ++letter)
    {
        int present = 0;
        for (size_t i = 0; i < state->catalog.count; ++i)
        {
            int index = state->sorted_songs[i].index;
            present |= catalog_title_matches(state->catalog.titles[index], state->search_query) &&
                       catalog_title_group(state->catalog.titles[index]) == *letter;
        }
        char label[2] = {*letter, 0};
        Node jump = element(alphabet, present ? "a" : "span", "index-letter", label);
        if (!present)
        {
            continue;
        }
        char id[24], anchor[28];
        snprintf(id, sizeof(id), "letter-%c", *letter == '#' ? '0' : *letter);
        snprintf(anchor, sizeof(anchor), "#%s", id);
        dom_attr(jump, "href", anchor);
        Node group = element(groups, "section", "index-group", NULL);
        dom_attr(group, "id", id);
        element(group, "h2", NULL, label);
        Node list = element(group, "ul", "index-list", NULL);
        for (size_t i = 0; i < state->catalog.count; ++i)
        {
            int index = state->sorted_songs[i].index;
            if (!catalog_title_matches(state->catalog.titles[index], state->search_query) ||
                catalog_title_group(state->catalog.titles[index]) != *letter)
            {
                continue;
            }
            char path[192];
            snprintf(path, sizeof(path), "/s/%s/", state->catalog.slugs[index]);
            Node item = element(list, "li", NULL, NULL);
            view_route_link(item, NULL, state->catalog.titles[index], path, APP_SELECT_SONG, index);
        }
    }
    if (!count)
    {
        element(groups, "p", "index-empty", ONLYC_STRING_CATALOG_EMPTY);
    }
}

static void mount(void* context, Node page)
{
    CatalogViewImpl* view = context;
    const ApplicationState* state = view->state;
    Node heading = element(page, "div", "index-heading", NULL);
    element(heading, "h1", NULL, ONLYC_STRING_CATALOG_TITLE);
    Node search = view_field(heading,
                             "song-search",
                             ONLYC_STRING_CATALOG_SEARCH,
                             state->search_query,
                             0,
                             sizeof(state->search_query) - 1);
    dom_attr(search, "type", "search");
    dom_attr(search, "autocomplete", "off");
    view->search = search;
    Node results = element(page, "div", "song-index", NULL);
    dom_attr(results, "id", "song-index");
    view->results = results;
    render_results(state, results);
}

static void unmount(void* context)
{
    CatalogViewImpl* view = context;
    view->search = 0;
    view->results = 0;
}

static void on_state_changed(void* context, ApplicationChange change)
{
    CatalogViewImpl* view = context;
    if (change == APPLICATION_INDEX_CHANGED && view->results)
    {
        char query[160];
        if (ui_read("song-search", query, sizeof(query)) < 0 ||
            strcmp(query, view->state->search_query))
        {
            dom_value(view->search, view->state->search_query);
        }
        dom_clear(view->results);
        render_results(view->state, view->results);
        theme_apply(view->state->artwork.color);
    }
}

void catalog_view_init(CatalogViewImpl* view, ApplicationController* controller)
{
    view->controller = controller;
    view->state = controller->state;
    application_state_subscribe(controller->state, &view->subscription, on_state_changed, view);
}

void catalog_view_destroy(CatalogViewImpl* view)
{
    application_state_unsubscribe(&view->subscription);
    unmount(view);
    view->controller = NULL;
    view->state = NULL;
}

CatalogView catalog_view_port(CatalogViewImpl* view)
{
    return (CatalogView){.context = view, .mount = mount, .unmount = unmount};
}

void catalog_view_input(CatalogViewImpl* view)
{
    char query[160];
    if (view->results && ui_read("song-search", query, sizeof(query)) >= 0)
    {
        application_controller_search(view->controller, query);
    }
}
