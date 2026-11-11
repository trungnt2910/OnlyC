#ifndef ONLYC_APP_MODEL_APPLICATIONSTATE_H
#define ONLYC_APP_MODEL_APPLICATIONSTATE_H

#include "app/model/EditorState.h"

typedef struct
{
    size_t count;
    const char* const* sources;
    const char* const* titles;
    const char* const* slugs;
} SongCatalog;

typedef struct
{
    int index;
    const char* title;
} CatalogEntry;

typedef enum
{
    PAGE_NOT_FOUND = -2,
    PAGE_HOME = -1
} ApplicationPage;

typedef enum
{
    APPLICATION_RENDER,
    APPLICATION_INDEX_CHANGED,
    APPLICATION_NOTICE_CHANGED,
    APPLICATION_READER_CHANGED,
    APPLICATION_EDITOR_CHANGED,
    APPLICATION_LINK_CHANGED,
    APPLICATION_THEME_CHANGED
} ApplicationChange;

typedef void (*ApplicationObserver)(void* context, ApplicationChange change);

typedef struct ApplicationSubscription ApplicationSubscription;
typedef struct ApplicationState ApplicationState;

/* Zero-initialize registrations. A subscriber owns its registration and detaches
 * before destruction. Registrations stay alive until active notifications return. */
struct ApplicationSubscription
{
    ApplicationState* state;
    ApplicationObserver observer;
    void* context;
    ApplicationSubscription* next;
};

typedef struct
{
    char url[1024];
    char seed[8];
    char color[8];
    unsigned request;
    int complete;
} ArtworkState;

/* One model per application instance. The catalog is borrowed and immutable;
 * parsed reader songs and the imported draft are owned by this model. */
struct ApplicationState
{
    SongCatalog catalog;
    Song** songs;
    CatalogEntry* sorted_songs;
    int* song_dirty;
    char location[256];
    char search_query[160];
    char notice[256];
    int import_in_progress;
    int active_song;
    int selected_section;
    size_t selected_line;
    int editing;
    int pending_changes;
    int has_unexported_changes;
    EditorState editor;
    ArtworkState artwork;
    ApplicationSubscription* subscriptions;
};

int application_state_init(ApplicationState* state, SongCatalog catalog);
void application_state_reset(ApplicationState* state);
void application_state_destroy(ApplicationState* state);
void application_state_subscribe(ApplicationState* state,
                                 ApplicationSubscription* subscription,
                                 ApplicationObserver observer,
                                 void* context);
void application_state_unsubscribe(ApplicationSubscription* subscription);
void application_state_notify(ApplicationState* state, ApplicationChange change);
void application_state_notice(ApplicationState* state, const char* text);
int catalog_title_matches(const char* title, const char* query);
char catalog_title_group(const char* title);

#endif
