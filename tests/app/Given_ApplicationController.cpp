#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "TestCatalog.h"
#include "app/controller/ApplicationController.h"
}

typedef struct
{
    char path[256];
    int renders;
    int notices;
    int themes;
    int focus_count;
    ViewFocus last_focus;
} ControllerContext;

static int location(void* context, char* path, int capacity)
{
    return snprintf(path, (size_t)capacity, "%s", ((ControllerContext*)context)->path);
}

static void navigate(void* context, const char* path)
{
    snprintf(((ControllerContext*)context)->path, 256, "%s", path);
}

static int confirm(void*, const char*)
{
    return 1;
}

static int download(void*, const char*, const Buffer*)
{
    return 1;
}

static void select_file(void*)
{
}

static void focus(void* context, ViewFocus target, size_t, size_t)
{
    ++((ControllerContext*)context)->focus_count;
    ((ControllerContext*)context)->last_focus = target;
}

static void observe(void* context, ApplicationChange change)
{
    ControllerContext* calls = (ControllerContext*)context;
    calls->renders += change == APPLICATION_RENDER;
    calls->notices += change == APPLICATION_NOTICE_CHANGED;
    calls->themes += change == APPLICATION_THEME_CHANGED;
}

class Given_ApplicationController : public ::testing::Test
{
protected:
    ApplicationState state = {};
    ApplicationState other_state = {};
    ApplicationController controller = {};
    ApplicationController other_controller = {};
    ApplicationSubscription subscription = {};
    ApplicationSubscription other_subscription = {};
    ControllerContext context = {};
    ControllerContext other_context = {};

    void SetUp() override
    {
        SongCatalog catalog = {.count = SONG_COUNT,
                               .sources = embedded_songs,
                               .titles = embedded_titles,
                               .slugs = embedded_slugs};
        ASSERT_TRUE(application_state_init(&state, catalog));
        ASSERT_TRUE(application_state_init(&other_state, catalog));
        ApplicationView view = {.context = &context, .focus = focus};
        ApplicationServices services = {.context = &context,
                                        .location = location,
                                        .navigate = navigate,
                                        .confirm = confirm,
                                        .download = download,
                                        .select_file = select_file,
                                        .artwork = nullptr};
        application_controller_init(&controller, &state, view, services);
        view.context = &other_context;
        services.context = &other_context;
        application_controller_init(&other_controller, &other_state, view, services);
        application_state_subscribe(&state, &subscription, observe, &context);
        application_state_subscribe(&other_state, &other_subscription, observe, &other_context);
    }

    void TearDown() override
    {
        application_state_destroy(&other_state);
        application_state_destroy(&state);
    }
};

TEST_F(Given_ApplicationController,
       When_OneInstanceNavigates_Then_OtherModelAndServicesAreUnchanged)
{
    strcpy(other_context.path, "/");

    application_controller_action(&controller, APP_SELECT_SONG, 0);

    EXPECT_EQ(state.active_song, 0);
    EXPECT_EQ(other_state.active_song, PAGE_HOME);
    EXPECT_EQ(other_state.songs[0], nullptr);
    EXPECT_STREQ(context.path, "/s/amber/");
    EXPECT_STREQ(other_context.path, "/");
    EXPECT_EQ(context.renders, 1);
    EXPECT_EQ(other_context.renders, 0);
}

TEST_F(Given_ApplicationController, When_SongChanges_Then_OnlyOnePublishedSongRemainsParsed)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);
    ASSERT_NE(state.songs[0], nullptr);

    application_controller_action(&controller, APP_SELECT_SONG, 1);

    EXPECT_EQ(state.songs[0], nullptr);
    EXPECT_NE(state.songs[1], nullptr);
    EXPECT_EQ(state.active_song, 1);
}

TEST_F(Given_ApplicationController, When_ImportSucceeds_Then_DraftIsOwnedAndCatalogRemainsBorrowed)
{
    const char* source = embedded_songs[0];

    application_controller_import(&controller, source, strlen(source));

    EXPECT_EQ(state.active_song, SONG_COUNT);
    EXPECT_TRUE(state.editing);
    EXPECT_STREQ(state.songs[SONG_COUNT]->title.original, "Amber");
    EXPECT_EQ(state.catalog.sources[0], source);
    EXPECT_EQ(other_state.songs[SONG_COUNT], nullptr);
    EXPECT_EQ(context.focus_count, 1);
}

TEST_F(Given_ApplicationController, When_ImportFails_Then_ReaderAndSelectionArePreserved)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);
    Song* original = state.songs[0];
    state.selected_section = 0;

    application_controller_import(&controller, "invalid: [", 10);

    EXPECT_EQ(state.active_song, 0);
    EXPECT_EQ(state.songs[0], original);
    EXPECT_EQ(state.selected_section, 0);
    EXPECT_EQ(state.songs[SONG_COUNT], nullptr);
    EXPECT_NE(state.notice[0], '\0');
}

TEST_F(Given_ApplicationController, When_FileReadIsPending_Then_DuplicateImportDoesNotStart)
{
    ASSERT_TRUE(application_controller_request_file(&controller, "arbitrary.YAML", 100, 1));

    int accepted = application_controller_request_file(&controller, "another.yml", 100, 1);

    EXPECT_FALSE(accepted);
    EXPECT_TRUE(state.import_in_progress);
}

TEST_F(Given_ApplicationController, When_FileReadFails_Then_AnotherImportCanStart)
{
    ASSERT_TRUE(application_controller_request_file(&controller, "first.yaml", 100, 1));

    application_controller_file_error(&controller);
    int accepted = application_controller_request_file(&controller, "second.yaml", 100, 1);

    EXPECT_TRUE(accepted);
    EXPECT_STREQ(state.notice, "Unable to read the selected file.");
}

TEST_F(Given_ApplicationController, When_InvalidInput_Then_NavigationKeepsUnexportedDraft)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_changed(&controller, EDITOR_TRANSLATED, "Too few lines");

    application_controller_action(&controller, APP_HOME, 0);

    EXPECT_EQ(state.active_song, SONG_COUNT);
    EXPECT_TRUE(state.pending_changes);
    EXPECT_TRUE(state.has_unexported_changes);
    EXPECT_STREQ(context.path, "/#draft");
}

TEST_F(Given_ApplicationController, When_ObserverDetached_Then_ChangesDoNotCallDestroyedView)
{
    application_state_unsubscribe(&subscription);

    application_controller_action(&controller, APP_SELECT_SONG, 0);

    EXPECT_EQ(state.active_song, 0);
    EXPECT_EQ(context.renders, 0);
    EXPECT_EQ(context.notices, 0);
}

TEST_F(Given_ApplicationController, When_FieldChanges_Then_ModelOwnsPendingInputWithoutAView)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    char title[32] = "New title";

    application_controller_changed(&controller, EDITOR_TITLE, title);
    strcpy(title, "Reused input buffer");

    EXPECT_STREQ(editor_state_text(&state.editor, EDITOR_TITLE), "New title");
    EXPECT_STREQ(state.songs[SONG_COUNT]->title.original, "Amber");
    EXPECT_TRUE(state.pending_changes);
    EXPECT_TRUE(state.has_unexported_changes);
}

TEST_F(Given_ApplicationController, When_PendingInputApplied_Then_ValidatedSongChangesWithoutAView)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_changed(&controller, EDITOR_TITLE, "New title");

    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_STREQ(state.songs[SONG_COUNT]->title.original, "New title");
    EXPECT_FALSE(state.pending_changes);
    EXPECT_TRUE(state.has_unexported_changes);
}

TEST_F(Given_ApplicationController,
       When_InvalidUrlApplied_Then_RawInputAndValidatedSongRemainSeparate)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    char original[512];
    strcpy(original, state.songs[SONG_COUNT]->links[0].url);
    application_controller_link_changed(&controller, 0, 1, "javascript:alert(1)");

    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_STREQ(state.editor.links[0].url.data, "javascript:alert(1)");
    EXPECT_STREQ(state.songs[SONG_COUNT]->links[0].url, original);
    EXPECT_STREQ(state.notice, "Each link needs a label and an HTTP or HTTPS URL.");
    EXPECT_TRUE(state.pending_changes);
}

TEST_F(Given_ApplicationController, When_InvalidInputCorrected_Then_ApplyCommitsTheCorrection)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_changed(&controller, EDITOR_ANNOTATION_START, "");
    application_controller_action(&controller, APP_APPLY, 0);
    ASSERT_TRUE(state.pending_changes);

    application_controller_changed(&controller, EDITOR_ANNOTATION_START, "1");
    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_FALSE(state.pending_changes);
    EXPECT_EQ(state.songs[SONG_COUNT]->ranges[0].first_line, 1u);
}

TEST_F(Given_ApplicationController, When_Utf8ExceedsByteLimit_Then_FullInputSurvivesFailedApply)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    char title[241];
    for (int i = 0; i < 80; ++i)
    {
        memcpy(title + 3 * i, "\xe2\x99\xaa", 3);
    }
    title[240] = 0;
    application_controller_changed(&controller, EDITOR_TITLE, title);

    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_STREQ(editor_state_text(&state.editor, EDITOR_TITLE), title);
    EXPECT_STREQ(state.songs[SONG_COUNT]->title.original, "Amber");
    EXPECT_TRUE(state.pending_changes);
    EXPECT_NE(strstr(state.notice, "UTF-8 byte limit"), nullptr);
}

TEST_F(Given_ApplicationController, When_ReaderReceivesFieldChange_Then_PublishedSongIsUnchanged)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);

    application_controller_changed(&controller, EDITOR_TITLE, "Replacement");

    EXPECT_STREQ(state.songs[0]->title.original, "Amber");
    EXPECT_FALSE(state.pending_changes);
    EXPECT_FALSE(state.has_unexported_changes);
}

TEST_F(Given_ApplicationController, When_InputCannotBeRead_Then_ExportCannotCommitStaleText)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));

    application_controller_changed(&controller, EDITOR_ANNOTATION_START, nullptr);
    application_controller_action(&controller, APP_EXPORT, 0);

    EXPECT_TRUE(state.pending_changes);
    EXPECT_TRUE(state.has_unexported_changes);
    EXPECT_EQ(state.songs[SONG_COUNT]->ranges[0].first_line, 1u);
}

TEST_F(Given_ApplicationController, When_ArtworkMatchesSeed_Then_NoThemeOrDomRefreshOccurs)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);
    strcpy(state.artwork.url, "https://example.test/cover.jpg");
    strcpy(state.artwork.color, "#50a0c8");
    int renders = context.renders;

    application_controller_artwork_ready(&controller, state.artwork.request, "#55a2c5");

    EXPECT_STREQ(state.artwork.color, "#50a0c8");
    EXPECT_EQ(context.themes, 0);
    EXPECT_EQ(context.renders, renders);
    EXPECT_TRUE(state.artwork.complete);
}

TEST_F(Given_ApplicationController, When_ArtworkChanges_Then_OnlyThemeObserverIsNotified)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);
    strcpy(state.artwork.url, "https://example.test/cover.jpg");
    strcpy(state.artwork.color, "#50a0c8");
    int renders = context.renders;

    application_controller_artwork_ready(&controller, state.artwork.request, "#f06040");

    EXPECT_STREQ(state.artwork.color, "#f06040");
    EXPECT_EQ(context.themes, 1);
    EXPECT_EQ(context.renders, renders);
}

TEST_F(Given_ApplicationController, When_StaleOrFailedArtwork_Then_AuthoredColorSurvives)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);
    strcpy(state.artwork.url, "https://example.test/cover.jpg");
    strcpy(state.artwork.color, "#50a0c8");

    application_controller_artwork_ready(&controller, state.artwork.request - 1, "#f06040");
    bool stale_ignored = !state.artwork.complete;
    application_controller_artwork_ready(&controller, state.artwork.request, nullptr);

    EXPECT_TRUE(stale_ignored);
    EXPECT_STREQ(state.artwork.color, "#50a0c8");
    EXPECT_EQ(context.themes, 0);
    EXPECT_TRUE(state.artwork.complete);
}

TEST_F(Given_ApplicationController, When_ArtworkColorDetectedInDraft_Then_RawColorIsFilledForExport)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_changed(&controller, EDITOR_ARTWORK, "https://example.test/cover.jpg");
    application_controller_changed(&controller, EDITOR_COLOR, "");
    application_controller_action(&controller, APP_APPLY, 0);
    ASSERT_STREQ(state.songs[SONG_COUNT]->color, "");

    application_controller_artwork_ready(&controller, state.artwork.request, "#50A0C8");

    EXPECT_STREQ(editor_state_text(&state.editor, EDITOR_COLOR), "#50a0c8");
    EXPECT_STREQ(state.songs[SONG_COUNT]->color, "");
    EXPECT_TRUE(state.pending_changes);
    EXPECT_TRUE(state.has_unexported_changes);
}

TEST_F(Given_ApplicationController, When_AuthorEditsColorDuringDownload_Then_ManualInputSurvives)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_changed(&controller, EDITOR_ARTWORK, "https://example.test/cover.jpg");
    application_controller_action(&controller, APP_APPLY, 0);
    application_controller_changed(&controller, EDITOR_COLOR, "#123456");

    application_controller_artwork_ready(&controller, state.artwork.request, "#50a0c8");

    EXPECT_STREQ(editor_state_text(&state.editor, EDITOR_COLOR), "#123456");
    EXPECT_TRUE(state.pending_changes);
}

TEST_F(Given_ApplicationController, When_SameUnseededDraftImportedAgain_Then_DetectedColorIsReused)
{
    Buffer source = {};
    buffer_puts(&source, embedded_songs[1]);
    buffer_puts(&source, "\nartwork: https://example.test/cover.jpg\n");
    ASSERT_FALSE(source.failed);
    application_controller_import(&controller, source.data, source.len);
    application_controller_artwork_ready(&controller, state.artwork.request, "#50a0c8");
    unsigned request = state.artwork.request;

    application_controller_import(&controller, source.data, source.len);
    bool same_request = state.artwork.request == request;
    buffer_free(&source);

    EXPECT_TRUE(same_request);
    EXPECT_STREQ(editor_state_text(&state.editor, EDITOR_COLOR), "#50a0c8");
    EXPECT_TRUE(state.pending_changes);
}

TEST_F(Given_ApplicationController, When_SharedNoteEdited_Then_AllVerseReferencesSeeTheNewText)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));

    application_controller_changed(&controller, EDITOR_ANNOTATION, "Updated shared note");
    application_controller_action(&controller, APP_APPLY, 0);
    const Song* song = state.songs[SONG_COUNT];
    int first = annotation_for_line(song, 0, 1);
    int bridge = annotation_for_line(song, 2, 1);

    EXPECT_FALSE(state.pending_changes);
    EXPECT_EQ(first, bridge);
    EXPECT_STREQ(song->annotations[first].text, "Updated shared note");
    EXPECT_EQ(song->range_count, 3u);
}

TEST_F(Given_ApplicationController, When_SharedNoteDetached_Then_OtherVerseKeepsItsReference)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));

    application_controller_action(&controller, APP_REMOVE_ANNOTATION, 0);
    const Song* song = state.songs[SONG_COUNT];

    EXPECT_EQ(annotation_for_line(song, 0, 1), -1);
    EXPECT_GE(annotation_for_line(song, 2, 1), 0);
    EXPECT_GE(annotation_by_key(song, "note-1"), 0);
    EXPECT_TRUE(state.has_unexported_changes);
}

TEST_F(Given_ApplicationController, When_ExistingKeyChosenForNewRange_Then_SharedTextIsLoaded)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_action(&controller, APP_SELECT_LINE, 2);

    application_controller_changed(&controller, EDITOR_ANNOTATION_REF, "note-1");
    application_controller_action(&controller, APP_APPLY, 0);
    const Song* song = state.songs[SONG_COUNT];

    EXPECT_FALSE(state.pending_changes);
    EXPECT_EQ(annotation_for_line(song, 0, 3), annotation_for_line(song, 0, 1));
    EXPECT_EQ(song->annotation_count, 3u);
    EXPECT_EQ(song->range_count, 4u);
}

TEST_F(Given_ApplicationController,
       When_TitleEditedAndAnnotated_Then_TranslationAndReferencePersist)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_action(&controller, APP_SELECT_TITLE, 0);

    application_controller_changed(&controller, EDITOR_TITLE_TRANSLATED, "Translated Amber");
    application_controller_changed(&controller, EDITOR_ANNOTATION, "A title explanation.");
    application_controller_action(&controller, APP_APPLY, 0);
    const Song* song = state.songs[SONG_COUNT];

    EXPECT_FALSE(state.pending_changes);
    EXPECT_STREQ(song_title(song), "Translated Amber");
    EXPECT_STREQ(song->title.original, "Amber");
    EXPECT_STREQ(song_description(song), "A title explanation.");
    EXPECT_EQ(state.selected_section, TITLE_SECTION);
}

TEST_F(Given_ApplicationController, When_UnannotatedTitleGivenNote_Then_KeyIsCreated)
{
    application_controller_import(&controller, embedded_songs[1], strlen(embedded_songs[1]));
    application_controller_action(&controller, APP_SELECT_TITLE, 0);

    application_controller_changed(&controller, EDITOR_ANNOTATION, "A new title note.");
    application_controller_action(&controller, APP_APPLY, 0);
    const Song* song = state.songs[SONG_COUNT];

    EXPECT_FALSE(state.pending_changes);
    EXPECT_NE(song->title.annotation[0], 0);
    EXPECT_STREQ(song_description(song), "A new title note.");
}

TEST_F(Given_ApplicationController, When_LinksAddedAndRemoved_Then_ModelOwnsTheirLabelsAndUrls)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_action(&controller, APP_ADD_LINK, 0);

    application_controller_link_changed(&controller, 1, 0, "Website");
    application_controller_link_changed(&controller, 1, 1, "https://example.com");
    application_controller_action(&controller, APP_APPLY, 0);
    bool added = state.songs[SONG_COUNT]->link_count == 2;
    application_controller_action(&controller, APP_REMOVE_LINK, 0);
    application_controller_action(&controller, APP_APPLY, 0);
    const Song* song = state.songs[SONG_COUNT];

    EXPECT_TRUE(added);
    EXPECT_FALSE(state.pending_changes);
    EXPECT_EQ(song->link_count, 1u);
    EXPECT_STREQ(song->links[0].label, "Website");
    EXPECT_STREQ(song->links[0].url, "https://example.com");
}

TEST_F(Given_ApplicationController, When_IncompleteLinkRemoved_Then_ValidationCanProceed)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));
    application_controller_action(&controller, APP_ADD_LINK, 0);
    application_controller_action(&controller, APP_APPLY, 0);
    ASSERT_TRUE(state.pending_changes);

    application_controller_action(&controller, APP_REMOVE_LINK, 1);
    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_FALSE(state.pending_changes);
    EXPECT_EQ(state.songs[SONG_COUNT]->link_count, 1u);
}

TEST_F(Given_ApplicationController, When_LinkedRangeStartsInsideNote_Then_FirstOverlappingNoteOpens)
{
    strcpy(context.path, "/s/amber/#L2-L6");

    application_controller_location(&controller);

    EXPECT_EQ(state.active_song, 0);
    EXPECT_EQ(state.selected_section, 0);
    EXPECT_EQ(state.selected_line, 1u);
    EXPECT_FALSE(state.editing);
    EXPECT_STREQ(state.location, "/s/amber/#L2-L6");
    EXPECT_EQ(context.focus_count, 2);
    EXPECT_EQ(context.last_focus, VIEW_FOCUS_PANEL);
}

TEST_F(Given_ApplicationController,
       When_LinkStartsOnUnmarkedLine_Then_LaterNoteDoesNotTakePrecedence)
{
    strcpy(context.path, "/s/amber#L3-L7");

    application_controller_location(&controller);

    EXPECT_EQ(state.selected_section, 0);
    EXPECT_EQ(state.selected_line, 3u);
    EXPECT_STREQ(context.path, "/s/amber#L3-L7");
}

TEST_F(Given_ApplicationController, When_LineInLaterVerseLinked_Then_GlobalNumberFindsLocalLine)
{
    strcpy(context.path, "/amber.html#L7");

    application_controller_location(&controller);

    EXPECT_EQ(state.selected_section, 1);
    EXPECT_EQ(state.selected_line, 3u);
    EXPECT_EQ(context.focus_count, 2);
}

TEST_F(Given_ApplicationController, When_TitleFragmentOpened_Then_TitlePanelReceivesFocus)
{
    strcpy(context.path, "/s/amber/index.html#title");

    application_controller_location(&controller);

    EXPECT_EQ(state.selected_section, TITLE_SECTION);
    EXPECT_EQ(context.last_focus, VIEW_FOCUS_PANEL);
    EXPECT_EQ(context.focus_count, 2);
}

TEST_F(Given_ApplicationController, When_FragmentDoesNotOverlap_Then_SongLoadsWithoutSelection)
{
    strcpy(context.path, "/s/amber/#L11-L12");

    application_controller_location(&controller);

    EXPECT_EQ(state.active_song, 0);
    EXPECT_EQ(state.selected_section, -1);
    EXPECT_EQ(context.focus_count, 0);
    EXPECT_EQ(state.notice[0], 0);
}

TEST_F(Given_ApplicationController, When_DuplicateLocationEventArrives_Then_PanelIsNotRenderedAgain)
{
    strcpy(context.path, "/s/amber/#L9");
    application_controller_location(&controller);
    int renders = context.renders;
    int focus_calls = context.focus_count;

    application_controller_location(&controller);

    EXPECT_EQ(context.renders, renders);
    EXPECT_EQ(context.focus_count, focus_calls);
    EXPECT_EQ(state.selected_section, 2);
    EXPECT_EQ(state.selected_line, 1u);
}

TEST_F(Given_ApplicationController, When_LyricSelected_Then_AddressContainsTheWholeAnnotationRange)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);

    application_controller_action(&controller, APP_SELECT_LINE, 1);

    EXPECT_STREQ(context.path, "/s/amber/#L1-L2");
    EXPECT_STREQ(state.location, context.path);
}

TEST_F(Given_ApplicationController, When_UnmarkedLineSelected_Then_AddressContainsOnlyThatLine)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);

    application_controller_action(&controller, APP_SELECT_LINE, MAX_LINES * 2 + 1);

    EXPECT_STREQ(context.path, "/s/amber/#L10");
}

TEST_F(Given_ApplicationController, When_TitleSelectedThenClosed_Then_FragmentIsAddedThenRemoved)
{
    application_controller_action(&controller, APP_SELECT_SONG, 0);

    application_controller_action(&controller, APP_SELECT_TITLE, 0);
    bool title_link = !strcmp(context.path, "/s/amber/#title");
    application_controller_action(&controller, APP_CLOSE_PANEL, 0);

    EXPECT_TRUE(title_link);
    EXPECT_STREQ(context.path, "/s/amber/");
    EXPECT_EQ(state.selected_section, -1);
}

TEST_F(Given_ApplicationController, When_HistoryReturnsToEarlierFragment_Then_SelectionIsRestored)
{
    strcpy(context.path, "/s/amber/#L9");
    application_controller_location(&controller);
    application_controller_action(&controller, APP_SELECT_TITLE, 0);
    strcpy(context.path, "/s/amber/#L9");

    application_controller_location(&controller);

    EXPECT_EQ(state.selected_section, 2);
    EXPECT_EQ(state.selected_line, 1u);
    EXPECT_STREQ(state.location, "/s/amber/#L9");
}

TEST_F(Given_ApplicationController, When_DraftLineSelected_Then_DraftRouteIsPreserved)
{
    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));

    application_controller_action(&controller, APP_SELECT_LINE, 2);

    EXPECT_STREQ(context.path, "/#draft");
    EXPECT_TRUE(state.editing);
}

TEST_F(Given_ApplicationController, When_NonYamlFileRequested_Then_FileSelectionToastIsReported)
{
    const char* filename = "image.png";

    int accepted = application_controller_request_file(&controller, filename, 100, 1);

    EXPECT_FALSE(accepted);
    EXPECT_STREQ(state.notice, "Choose a .yaml or .yml file.");
    EXPECT_EQ(context.notices, 1);
}

TEST_F(Given_ApplicationController, When_ImportSucceedsAfterError_Then_ToastIsReplaced)
{
    application_controller_import(&controller, "invalid: [", 10);

    application_controller_import(&controller, embedded_songs[0], strlen(embedded_songs[0]));

    EXPECT_STREQ(state.notice, "YAML imported.");
}
