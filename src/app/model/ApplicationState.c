#include "app/model/ApplicationState.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_titles(const void* left, const void* right)
{
    const CatalogEntry* first = left;
    const CatalogEntry* second = right;
    const unsigned char* a = (const unsigned char*)first->title;
    const unsigned char* b = (const unsigned char*)second->title;
    while (*a && *b && tolower(*a) == tolower(*b))
    {
        ++a;
        ++b;
    }
    return tolower(*a) - tolower(*b);
}

int application_state_init(ApplicationState* state, SongCatalog catalog)
{
    memset(state, 0, sizeof(*state));
    if (!catalog.count || catalog.count >= INT_MAX || !catalog.sources || !catalog.titles ||
        !catalog.slugs)
    {
        return 0;
    }
    for (size_t i = 0; i < catalog.count; ++i)
    {
        if (!catalog.sources[i] || !catalog.titles[i] || !catalog.slugs[i])
        {
            return 0;
        }
    }
    state->catalog = catalog;
    state->songs = calloc(catalog.count + 1, sizeof(*state->songs));
    state->song_dirty = calloc(catalog.count + 1, sizeof(*state->song_dirty));
    state->sorted_songs = calloc(catalog.count, sizeof(*state->sorted_songs));
    if (!state->songs || !state->song_dirty || !state->sorted_songs)
    {
        application_state_destroy(state);
        return 0;
    }
    for (size_t i = 0; i < catalog.count; ++i)
    {
        state->sorted_songs[i].index = (int)i;
        state->sorted_songs[i].title = catalog.titles[i];
    }
    qsort(state->sorted_songs, catalog.count, sizeof(*state->sorted_songs), compare_titles);
    application_state_reset(state);
    return 1;
}

void application_state_reset(ApplicationState* state)
{
    editor_state_destroy(&state->editor);
    for (size_t i = 0; i <= state->catalog.count; ++i)
    {
        free(state->songs[i]);
        state->songs[i] = NULL;
        state->song_dirty[i] = 0;
    }
    state->active_song = PAGE_HOME;
    state->selected_section = -1;
    state->selected_line = 1;
    state->location[0] = '\0';
    state->search_query[0] = '\0';
    state->notice[0] = '\0';
    state->import_in_progress = 0;
    state->editing = 0;
    state->pending_changes = 0;
    state->has_unexported_changes = 0;
    state->artwork.url[0] = 0;
    state->artwork.seed[0] = 0;
    state->artwork.color[0] = 0;
    state->artwork.complete = 0;
    ++state->artwork.request;
}

void application_state_destroy(ApplicationState* state)
{
    while (state->subscriptions)
    {
        application_state_unsubscribe(state->subscriptions);
    }
    editor_state_destroy(&state->editor);
    if (state->songs)
    {
        for (size_t i = 0; i <= state->catalog.count; ++i)
        {
            free(state->songs[i]);
        }
    }
    free(state->songs);
    free(state->song_dirty);
    free(state->sorted_songs);
    memset(state, 0, sizeof(*state));
}

void application_state_subscribe(ApplicationState* state,
                                 ApplicationSubscription* subscription,
                                 ApplicationObserver observer,
                                 void* context)
{
    application_state_unsubscribe(subscription);
    *subscription =
        (ApplicationSubscription){.state = state, .observer = observer, .context = context};
    ApplicationSubscription** tail = &state->subscriptions;
    while (*tail)
    {
        tail = &(*tail)->next;
    }
    *tail = subscription;
}

void application_state_unsubscribe(ApplicationSubscription* subscription)
{
    if (!subscription->state)
    {
        return;
    }
    ApplicationSubscription** current = &subscription->state->subscriptions;
    while (*current && *current != subscription)
    {
        current = &(*current)->next;
    }
    if (*current)
    {
        *current = subscription->next;
    }
    subscription->state = NULL;
    subscription->observer = NULL;
}

void application_state_notify(ApplicationState* state, ApplicationChange change)
{
    /* Registrations remain alive until notification returns; keeping next on
     * detach lets callbacks unsubscribe themselves or another observer. */
    ApplicationSubscription* subscription = state->subscriptions;
    while (subscription)
    {
        ApplicationSubscription* next = subscription->next;
        if (subscription->state == state && subscription->observer)
        {
            subscription->observer(subscription->context, change);
        }
        subscription = next;
    }
}

void application_state_notice(ApplicationState* state, const char* text)
{
    snprintf(state->notice, sizeof(state->notice), "%s", text);
    application_state_notify(state, APPLICATION_NOTICE_CHANGED);
}

int catalog_title_matches(const char* title, const char* query)
{
    if (!*query)
    {
        return 1;
    }
    for (const unsigned char* start = (const unsigned char*)title; *start; ++start)
    {
        const unsigned char* text = start;
        const unsigned char* term = (const unsigned char*)query;
        while (*text && *term && tolower(*text) == tolower(*term))
        {
            ++text;
            ++term;
        }
        if (!*term)
        {
            return 1;
        }
    }
    return 0;
}

char catalog_title_group(const char* title)
{
    int initial = toupper((unsigned char)title[0]);
    return initial >= 'A' && initial <= 'Z' ? (char)initial : '#';
}
