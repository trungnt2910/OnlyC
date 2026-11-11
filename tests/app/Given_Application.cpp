#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "TestCatalog.h"
#include "app/controller/ApplicationController.h"
#include "app/view/impl/ApplicationViewImpl.h"
#include "core/Song.h"
#include "fakes/DomMock.h"
#include "platform/web/Services.h"
#include "strings/Resources.h"
}

static Buffer downloaded;
static Song draft;
static char notice[256], focus[80], route[256];
static int confirm_calls;
static int confirm_answer = 1;
static char note_text[8192], note_start[16], note_end[16], search_text[160];
extern "C"
{
    int ui_read(const char* id, char* text, int capacity)
    {
        const char* value = test_input_value(id);
        if (strlen(value) >= (size_t)capacity)
        {
            return -1;
        }
        strcpy(text, value);
        return (int)strlen(value);
    }

    int ui_path(char* text, int capacity)
    {
        return snprintf(text, (size_t)capacity, "%s", route);
    }

    void ui_title(const char*)
    {
    }

    void ui_route(const char* path)
    {
        snprintf(route, sizeof(route), "%s", path);
    }

    void ui_focus(const char* id)
    {
        snprintf(focus, sizeof(focus), "%s", id);
    }

    void ui_set_text(const char*, const char* text)
    {
        snprintf(notice, sizeof(notice), "%s", text);
    }

    void ui_toast(const char* text)
    {
        ui_set_text("#status", text);
    }

    int ui_blob_url(const char* text, unsigned len, const char*, char* url, int capacity)
    {
        buffer_free(&downloaded);
        buffer_append(&downloaded, text, len);
        return snprintf(url, (size_t)capacity, "blob:export");
    }

    void ui_revoke_url(const char*, int)
    {
    }

    int ui_confirm(const char*)
    {
        ++confirm_calls;
        return confirm_answer;
    }
}

static void edit_input(BrowserApplicationView* view, const char* id, const char* value)
{
    char selector[96];
    snprintf(selector, sizeof(selector), "#%s", id);
    dom_value(dom_find(selector), value);
    snprintf(test_event_id, sizeof(test_event_id), "%s", id);
    browser_view_event(view, APP_EVENT_INPUT);
}

static void open_draft(ApplicationController* controller)
{
    application_controller_import(controller, embedded_songs[0], strlen(embedded_songs[0]));
}

class Given_LyricsEditor : public ::testing::Test
{
protected:
    ApplicationState state = {};
    ApplicationController controller = {};
    BrowserApplicationView view = {};

    void SetUp() override
    {
        char error[256];
        test_set_width(1440);
        test_prevented = 0;
        test_read_requests = 0;
        test_event_id[0] = 0;
        confirm_calls = 0;
        confirm_answer = 1;
        strcpy(route, "/s/amber/");
        search_text[0] = 0;
        test_has_files = 0;
        test_drop_count = 0;
        test_drop_bytes = 0;
        test_drop_reads = 0;
        test_plain_click = 1;
        test_drop_filename[0] = 0;
        notice[0] = 0;
        focus[0] = 0;
        ASSERT_TRUE(
            song_parse(&draft, embedded_songs[0], strlen(embedded_songs[0]), error, sizeof(error)));
        strcpy(note_text, draft.annotations[0].text);
        strcpy(note_start, "1");
        strcpy(note_end, "2");
        SongCatalog catalog = {.count = SONG_COUNT,
                               .sources = embedded_songs,
                               .titles = embedded_titles,
                               .slugs = embedded_slugs};
        ASSERT_TRUE(application_state_init(&state, catalog));
        application_controller_init(
            &controller, &state, browser_view_port(&view), browser_services());
        browser_view_attach(&view, &controller);
        application_controller_start(&controller);
    }

    void TearDown() override
    {
        browser_view_detach(&view);
        application_state_destroy(&state);
        buffer_free(&downloaded);
    }
};

TEST_F(Given_LyricsEditor, When_FileInputChangesBeforeImport_Then_NoSpuriousUnsavedPromptAppears)
{
    open_draft(&controller);
    const char* input = embedded_songs[1];

    edit_input(&view, "import-file", "");
    application_controller_import(&controller, input, strlen(input));

    EXPECT_EQ(confirm_calls, 0);
    EXPECT_EQ(state.has_unexported_changes, 0);
    EXPECT_NE(strstr(test_document(), "Frost"), nullptr);
    EXPECT_STREQ(focus, "panel");
}

TEST_F(Given_LyricsEditor, When_PublishedSongOpens_Then_DiscussionUsesStableSongTerm)
{
    strcpy(route, "/s/frost/#L1");

    application_controller_location(&controller);

    EXPECT_STREQ(test_discussion_term, "song:frost");
    EXPECT_STREQ(test_discussion_backlink, ONLYC_STRING_GISCUS_SITE_ORIGIN "/s/frost/");
}

TEST_F(Given_LyricsEditor, When_LineSelected_Then_DiscussionIdentityIsUnchanged)
{
    ASSERT_STREQ(test_discussion_term, "song:amber");

    application_controller_action(&controller, APP_SELECT_LINE, 2);

    EXPECT_STREQ(test_discussion_term, "song:amber");
    EXPECT_STREQ(test_discussion_description, song_description(state.songs[0]));
}

TEST_F(Given_LyricsEditor, When_HomeOpened_Then_DiscussionIsHidden)
{
    ASSERT_STREQ(test_discussion_term, "song:amber");

    application_controller_action(&controller, APP_HOME, 0);

    EXPECT_STREQ(test_discussion_term, "");
}

TEST_F(Given_LyricsEditor, When_DraftImported_Then_NoPublicDiscussionIsShown)
{
    ASSERT_STREQ(test_discussion_term, "song:amber");

    open_draft(&controller);

    EXPECT_STREQ(test_discussion_term, "");
}

TEST_F(Given_LyricsEditor, When_DraftPreviewOpened_Then_DiscussionUsesTheDraftSlug)
{
    open_draft(&controller);
    application_controller_changed(&controller, EDITOR_SLUG, "preview-song");

    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);

    EXPECT_STREQ(test_discussion_term, "song:preview-song");
    EXPECT_STREQ(test_discussion_backlink, ONLYC_STRING_GISCUS_SITE_ORIGIN "/s/preview-song/");
    EXPECT_FALSE(state.editing);
}

TEST_F(Given_LyricsEditor, When_DraftPreviewReturnsToEditing_Then_DiscussionIsHidden)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);
    ASSERT_STREQ(test_discussion_term, "song:amber");

    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);

    EXPECT_STREQ(test_discussion_term, "");
    EXPECT_TRUE(state.editing);
}

TEST_F(Given_LyricsEditor, When_TranslationEdited_Then_ApplyUpdatesTheReader)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    strcpy(draft.sections[0].translated, "Bản dịch mới\nDòng thứ hai");
    strcpy(draft.sections[0].original, "First line\nSecond line");

    edit_input(&view, "original", draft.sections[0].original);
    edit_input(&view, "translated", draft.sections[0].translated);
    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_NE(strstr(test_document(), "Bản dịch mới"), nullptr);
    EXPECT_NE(strstr(test_document(), "Dòng thứ hai"), nullptr);
    EXPECT_EQ(state.has_unexported_changes, 1);
}

TEST_F(Given_LyricsEditor, When_EditedSongExported_Then_DownloadContainsChangesAndDirtyFlagClears)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    strcpy(draft.sections[0].translated, "A saved translation\nSecond line");
    strcpy(draft.sections[0].original, "First line\nSecond line");
    Song imported = {};
    char error[256];

    edit_input(&view, "original", draft.sections[0].original);
    edit_input(&view, "translated", draft.sections[0].translated);
    application_controller_action(&controller, APP_EXPORT, 0);
    int ok = song_parse(
        &imported, downloaded.data ? downloaded.data : "", downloaded.len, error, sizeof(error));

    EXPECT_TRUE(ok);
    EXPECT_STREQ(imported.sections[0].translated, "A saved translation\nSecond line");
    EXPECT_EQ(state.has_unexported_changes, 0);
}

TEST_F(Given_LyricsEditor, When_SwitchingSongsWithEdits_Then_InMemoryWorkIsPreserved)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    strcpy(draft.sections[0].translated, "Remember this translation\nSecond line");
    strcpy(draft.sections[0].original, "First line\nSecond line");

    edit_input(&view, "original", draft.sections[0].original);
    edit_input(&view, "translated", draft.sections[0].translated);
    application_controller_action(&controller, APP_SELECT_SONG, 1);
    application_controller_action(&controller, APP_RESUME_DRAFT, 0);

    EXPECT_NE(strstr(test_document(), "Remember this translation"), nullptr);
    EXPECT_EQ(state.has_unexported_changes, 1);
    EXPECT_STREQ(route, "/#draft");
}

TEST_F(Given_LyricsEditor, When_InvalidImport_Then_ReaderRemainsUnchanged)
{
    Buffer before = {};
    buffer_puts(&before, test_document());
    const char* input = "invalid: [unterminated";

    application_controller_import(&controller, input, strlen(input));
    int same = strcmp(before.data, test_document()) == 0;
    buffer_free(&before);

    EXPECT_TRUE(same);
    EXPECT_NE(notice[0], 0);
}

TEST_F(Given_LyricsEditor, When_ImportReplacesUnexportedWork_Then_ConfirmationIsRequested)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    edit_input(&view, "annotation-start", note_start);
    edit_input(&view, "annotation-end", note_end);
    edit_input(&view, "annotation", note_text);

    application_controller_import(&controller, embedded_songs[1], strlen(embedded_songs[1]));

    EXPECT_EQ(confirm_calls, 1);
    EXPECT_EQ(state.has_unexported_changes, 0);
}

TEST_F(Given_LyricsEditor, When_InvalidMetadataApplied_Then_InputIsNotDiscarded)
{
    open_draft(&controller);
    strcpy(draft.links[0].url, "javascript:alert(1)");

    edit_input(&view, "link-url-0", draft.links[0].url);
    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_STREQ(test_input_value("link-url-0"), "javascript:alert(1)");
    EXPECT_STREQ(state.editor.links[0].url.data, "javascript:alert(1)");
    EXPECT_EQ(state.has_unexported_changes, 1);
    EXPECT_STREQ(notice, "Each link needs a label and an HTTP or HTTPS URL.");
}

TEST_F(Given_LyricsEditor, When_AnnotationPreviewUpdated_Then_MarkdownIsRenderedSafely)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    const char* input = "**Live** <script>alert(1)</script>";

    application_controller_changed(&controller, EDITOR_ANNOTATION, input);

    EXPECT_NE(strstr(test_subtree("#markdown-preview"), "<strong>Live</strong>"), nullptr);
    EXPECT_EQ(strstr(test_subtree("#markdown-preview"), "<script>"), nullptr);
}

TEST_F(Given_LyricsEditor, When_AnnotationClosed_Then_FocusReturnsToLine)
{
    application_controller_action(&controller, APP_SELECT_LINE, 0);

    application_controller_action(&controller, APP_CLOSE_PANEL, 0);

    EXPECT_STREQ(focus, "line-0-1");
    EXPECT_EQ(strstr(test_document(), "id=\"panel\""), nullptr);
}

TEST_F(Given_LyricsEditor, When_ViewportBecomesMobile_Then_CSetsSheetProperties)
{
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    test_set_width(390);

    browser_view_event(&view, APP_EVENT_RESIZE);

    EXPECT_STREQ(test_style(".annotation-panel", "position"), "fixed");
    EXPECT_STREQ(test_style("body", "overflow"), "hidden");
    EXPECT_STREQ(test_style(".reading-grid", "display"), "block");
}

TEST_F(Given_LyricsEditor, When_ViewportReturnsToDesktop_Then_CRestoresSidebarProperties)
{
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    test_set_width(390);
    browser_view_event(&view, APP_EVENT_RESIZE);
    test_set_width(1440);

    browser_view_event(&view, APP_EVENT_RESIZE);

    EXPECT_STREQ(test_style(".annotation-panel", "position"), "sticky");
    EXPECT_STREQ(test_style("body", "overflow"), "visible");
    EXPECT_STREQ(test_style(".reading-grid", "display"), "grid");
}

TEST_F(Given_LyricsEditor, When_LastSheetControlReceivesTab_Then_CWrapsFocus)
{
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    test_set_width(390);
    strcpy(test_key, "Tab");
    test_shift = 0;
    test_active_index = 2;

    browser_view_event(&view, APP_EVENT_KEY);

    EXPECT_EQ(test_active_index, 0);
    EXPECT_EQ(test_prevented, 1);
}

TEST_F(Given_LyricsEditor, When_UnexportedWorkIsPresent_Then_CPreventsUnload)
{
    open_draft(&controller);
    edit_input(&view, "title", draft.title.original);

    browser_view_event(&view, APP_EVENT_UNLOAD);

    EXPECT_EQ(test_prevented, 1);
}

TEST_F(Given_LyricsEditor, When_FileIsTooLarge_Then_CRejectsItBeforeReading)
{
    open_draft(&controller);
    strcpy(test_event_id, "import-file");
    test_file_bytes = MAX_IMPORT + 1;

    browser_view_event(&view, APP_EVENT_FILE);

    EXPECT_EQ(test_read_requests, 0);
    EXPECT_STREQ(notice, "Choose a nonempty YAML file no larger than 1 MiB.");
}

TEST_F(Given_LyricsEditor,
       When_UnannotatedLineSelected_Then_WholeVerseAppearsWithOnlyClickedLineHighlighted)
{
    const char* expected_original = "Sed do eiusmod tempor incididunt.";

    application_controller_action(&controller, APP_SELECT_LINE, 2);

    EXPECT_NE(strstr(test_subtree("#panel"), expected_original), nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "class=\"original-line selected\" data-line=\"3\""),
              nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "class=\"original-line\" data-line=\"1\""), nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "Lorem ipsum dolor sit amet."), nullptr);
    EXPECT_EQ(strstr(test_subtree("#panel"), "class=\"markdown\""), nullptr);
    EXPECT_EQ(strstr(test_subtree("#panel"), "No annotation"), nullptr);
}

TEST_F(Given_LyricsEditor, When_AnnotatedLineSelected_Then_WholeVerseAndNoteAppear)
{
    const char* expected_original = "Consectetur adipiscing elit.";

    application_controller_action(&controller, APP_SELECT_LINE, 1);

    EXPECT_NE(strstr(test_subtree("#panel"), expected_original), nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "<strong>Lorem ipsum</strong>"), nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "Sed do eiusmod tempor incididunt."), nullptr);
    EXPECT_NE(strstr(test_subtree("#line-0-1"), "annotated selected"), nullptr);
    EXPECT_NE(strstr(test_subtree("#line-0-2"), "annotated selected"), nullptr);
}

TEST_F(Given_LyricsEditor, When_ReaderRenders_Then_EachClickableElementContainsOnlyOneLine)
{
    const char* first_line = "Sed ut perspiciatis unde omnis.";

    const char* line = test_subtree("#line-0-1");
    int has_first = strstr(line, first_line) != nullptr;
    int has_second = strstr(line, "Iste natus error sit voluptatem.") != nullptr;

    EXPECT_TRUE(has_first);
    EXPECT_FALSE(has_second);
    EXPECT_EQ(strstr(test_document(), "class=\"passage"), nullptr);
}

TEST_F(Given_LyricsEditor,
       When_NewAnnotationAppliedToUnannotatedRange_Then_ExportContainsWholeLineRange)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 2);
    strcpy(note_text, "New **note**");
    strcpy(note_start, "3");
    strcpy(note_end, "4");
    Song imported = {};
    char error[256];

    edit_input(&view, "annotation-start", note_start);
    edit_input(&view, "annotation-end", note_end);
    edit_input(&view, "annotation", note_text);
    application_controller_action(&controller, APP_EXPORT, 0);
    int ok = song_parse(
        &imported, downloaded.data ? downloaded.data : "", downloaded.len, error, sizeof(error));
    int index = annotation_range_for_line(&imported, 0, 4);
    size_t first = index < 0 ? 0 : imported.ranges[index].first_line;
    size_t last = index < 0 ? 0 : imported.ranges[index].last_line;

    EXPECT_TRUE(ok);
    EXPECT_GE(index, 0);
    EXPECT_EQ(first, 3u);
    EXPECT_EQ(last, 4u);
    EXPECT_EQ(state.has_unexported_changes, 0);
}

TEST_F(Given_LyricsEditor,
       When_NewRangeOverlapsAnExistingNote_Then_ApplyPreservesInputsAndReportsError)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 2);
    strcpy(note_text, "Conflicting note");
    strcpy(note_start, "2");
    strcpy(note_end, "3");

    edit_input(&view, "annotation-start", note_start);
    edit_input(&view, "annotation-end", note_end);
    edit_input(&view, "annotation", note_text);
    application_controller_action(&controller, APP_APPLY, 0);

    EXPECT_STREQ(editor_state_text(&state.editor, EDITOR_ANNOTATION), "Conflicting note");
    EXPECT_EQ(state.songs[SONG_COUNT]->annotation_count, draft.annotation_count);
    EXPECT_STREQ(notice, "Annotation ranges cannot overlap within a section.");
    EXPECT_EQ(state.has_unexported_changes, 1);
}

TEST_F(Given_LyricsEditor, When_AnnotationDetached_Then_WholeVerseRemainsWithoutTheNote)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 1);

    application_controller_action(&controller, APP_REMOVE_ANNOTATION, 0);
    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);

    EXPECT_NE(strstr(test_subtree("#panel"), "Consectetur adipiscing elit."), nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "Lorem ipsum dolor sit amet."), nullptr);
    EXPECT_EQ(strstr(test_subtree("#panel"), "class=\"markdown\""), nullptr);
    EXPECT_EQ(state.has_unexported_changes, 1);
}

TEST_F(Given_LyricsEditor, When_ExistingRangeMoves_Then_SelectionFollowsTheNewRange)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 0);
    strcpy(note_start, "3");
    strcpy(note_end, "4");

    edit_input(&view, "annotation-start", note_start);
    edit_input(&view, "annotation-end", note_end);
    application_controller_action(&controller, APP_APPLY, 0);
    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);

    EXPECT_NE(strstr(test_subtree("#panel"), "Ut labore et dolore magna aliqua."), nullptr);
    EXPECT_NE(strstr(test_subtree("#line-0-3"), "annotated selected"), nullptr);
    EXPECT_NE(strstr(test_subtree("#line-0-4"), "annotated selected"), nullptr);
}

TEST_F(Given_LyricsEditor, When_HomeOpened_Then_AlphabeticalIndexReplacesLyricsAndEditor)
{
    strcpy(route, "/");

    application_controller_location(&controller);

    EXPECT_NE(strstr(test_document(), "id=\"song-index\""), nullptr);
    EXPECT_NE(strstr(test_document(), "id=\"letter-A\""), nullptr);
    EXPECT_NE(strstr(test_document(), "id=\"letter-F\""), nullptr);
    EXPECT_NE(strstr(test_document(), "href=\"/s/amber/\""), nullptr);
    EXPECT_EQ(strstr(test_document(), "class=\"lyrics\""), nullptr);
    EXPECT_EQ(strstr(test_document(), "song-tab"), nullptr);
    EXPECT_EQ(strstr(test_document(), "Edit translation"), nullptr);
    EXPECT_EQ(strstr(test_document(), "id=\"import-file\""), nullptr);
}

TEST_F(Given_LyricsEditor, When_PublicReaderReceivesEditAction_Then_EditorRemainsHidden)
{
    ASSERT_NE(strstr(test_document(), "class=\"lyrics\""), nullptr);

    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);

    EXPECT_EQ(strstr(test_document(), "editor-meta"), nullptr);
    EXPECT_EQ(strstr(test_document(), "id=\"import-file\""), nullptr);
    EXPECT_NE(strstr(test_document(), "← All songs"), nullptr);
}

TEST_F(Given_LyricsEditor, When_SearchUsesMixedCase_Then_OnlyMatchingSongIsListed)
{
    application_controller_action(&controller, APP_HOME, 0);
    strcpy(test_event_id, "song-search");
    strcpy(search_text, "fRo");
    dom_value(dom_find("#song-search"), search_text);

    browser_view_event(&view, APP_EVENT_INPUT);

    EXPECT_NE(strstr(test_subtree("#song-index"), "Frost"), nullptr);
    EXPECT_EQ(strstr(test_subtree("#song-index"), "Amber"), nullptr);
    EXPECT_NE(strstr(test_subtree("#song-index"), "1 song"), nullptr);
}

TEST_F(Given_LyricsEditor, When_SearchHasNoMatches_Then_AnEmptyResultIsShown)
{
    application_controller_action(&controller, APP_HOME, 0);
    strcpy(test_event_id, "song-search");
    strcpy(search_text, "No such song");
    dom_value(dom_find("#song-search"), search_text);

    browser_view_event(&view, APP_EVENT_INPUT);

    EXPECT_NE(strstr(test_subtree("#song-index"), "No songs found."), nullptr);
    EXPECT_EQ(strstr(test_subtree("#song-index"), "href=\"/s/"), nullptr);
}

TEST_F(Given_LyricsEditor, When_UnknownRouteOpened_Then_NoArbitrarySongIsSelected)
{
    strcpy(route, "/s/missing/");

    application_controller_location(&controller);

    EXPECT_NE(strstr(test_document(), "Song not found"), nullptr);
    EXPECT_EQ(strstr(test_document(), "class=\"lyrics\""), nullptr);
}

TEST_F(Given_LyricsEditor, When_DraftRouteOpenedWithoutImport_Then_IndexIsShown)
{
    strcpy(route, "/#draft");

    application_controller_location(&controller);

    EXPECT_NE(strstr(test_document(), "id=\"song-index\""), nullptr);
    EXPECT_EQ(strstr(test_document(), "editor-meta"), nullptr);
}

static void prepare_drop(const char* filename, int count, int bytes)
{
    strcpy(test_drop_filename, filename);
    test_has_files = 1;
    test_drop_count = count;
    test_drop_bytes = bytes;
}

TEST_F(Given_LyricsEditor, When_YamlDroppedOnHome_Then_ImportOpensTheDraftEditor)
{
    application_controller_action(&controller, APP_HOME, 0);
    prepare_drop("random.YAML", 1, (int)strlen(embedded_songs[1]));

    browser_view_event(&view, APP_EVENT_DROP);
    application_controller_import(&controller, embedded_songs[1], strlen(embedded_songs[1]));

    EXPECT_EQ(test_drop_reads, 1);
    EXPECT_EQ(test_prevented, 1);
    EXPECT_NE(strstr(test_document(), "editor-meta"), nullptr);
    EXPECT_NE(strstr(test_document(), "Frost"), nullptr);
    EXPECT_STREQ(route, "/#draft");
    EXPECT_EQ(state.has_unexported_changes, 0);
}

TEST_F(Given_LyricsEditor, When_DroppedYamlIsInvalid_Then_PublicReaderAndHiddenEditorArePreserved)
{
    prepare_drop("broken.yaml", 1, 10);
    Buffer before = {};
    buffer_puts(&before, test_document());

    browser_view_event(&view, APP_EVENT_DROP);
    application_controller_import(&controller, "invalid: [", 10);
    int unchanged = !strcmp(before.data, test_document());
    buffer_free(&before);

    EXPECT_TRUE(unchanged);
    EXPECT_EQ(strstr(test_document(), "editor-meta"), nullptr);
    EXPECT_NE(notice[0], 0);
}

TEST_F(Given_LyricsEditor, When_NonYamlFileDropped_Then_BrowserNavigationIsPreventedWithoutReading)
{
    prepare_drop("notes.txt", 1, 100);

    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_prevented, 1);
    EXPECT_EQ(test_drop_reads, 0);
    EXPECT_STREQ(notice, "Choose a .yaml or .yml file.");
}

TEST_F(Given_LyricsEditor, When_MultipleFilesDropped_Then_NothingIsImported)
{
    prepare_drop("one.yaml", 2, 100);

    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_drop_reads, 0);
    EXPECT_STREQ(notice, "Drop one YAML file at a time.");
}

TEST_F(Given_LyricsEditor, When_OversizedFileDropped_Then_ReadingIsRejected)
{
    prepare_drop("large.yaml", 1, MAX_IMPORT + 1);

    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_drop_reads, 0);
    EXPECT_STREQ(notice, "Choose a nonempty YAML file no larger than 1 MiB.");
}

TEST_F(Given_LyricsEditor, When_FileDragEntersPage_Then_DropIsAllowedWithoutShowingEditor)
{
    test_has_files = 1;

    browser_view_event(&view, APP_EVENT_DRAG);

    EXPECT_EQ(test_prevented, 1);
    EXPECT_EQ(strstr(test_document(), "editor-meta"), nullptr);
}

TEST_F(Given_LyricsEditor, When_TextIsDragged_Then_DefaultBrowserBehaviorIsPreserved)
{
    test_has_files = 0;

    browser_view_event(&view, APP_EVENT_DRAG);
    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_prevented, 0);
    EXPECT_EQ(test_drop_reads, 0);
}

TEST_F(Given_LyricsEditor, When_AnotherDropArrivesDuringReading_Then_OnlyOneImportRuns)
{
    prepare_drop("one.yaml", 1, 100);

    browser_view_event(&view, APP_EVENT_DROP);
    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_drop_reads, 1);
}

TEST_F(Given_LyricsEditor, When_FileReadFails_Then_AnotherDropCanBeRead)
{
    prepare_drop("one.yaml", 1, 100);
    browser_view_event(&view, APP_EVENT_DROP);

    application_controller_file_error(&controller);
    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_drop_reads, 2);
}

TEST_F(Given_LyricsEditor, When_ImportedDraftHasSameSlug_Then_PublishedLyricsStayUnchanged)
{
    strcpy(draft.title.translated, "Local draft title");
    Buffer yaml = {};
    char error[256];
    ASSERT_TRUE(song_validate(&draft, error, sizeof(error)));
    ASSERT_TRUE(song_export(&draft, &yaml));

    application_controller_import(&controller, yaml.data, yaml.len);
    application_controller_action(&controller, APP_SELECT_SONG, 0);
    int unchanged = strstr(test_document(), "Local draft title") == nullptr;
    application_controller_action(&controller, APP_HOME, 0);
    int index_unchanged = strstr(test_subtree("#song-index"), "Local draft title") == nullptr;
    application_controller_action(&controller, APP_RESUME_DRAFT, 0);
    int draft_preserved = strstr(test_document(), "Local draft title") != nullptr;
    buffer_free(&yaml);

    EXPECT_TRUE(unchanged);
    EXPECT_TRUE(index_unchanged);
    EXPECT_TRUE(draft_preserved);
}

TEST_F(Given_LyricsEditor, When_ReturningHomeWithEdits_Then_DraftSurvivesNavigation)
{
    open_draft(&controller);
    strcpy(draft.title.translated, "Unsaved draft");
    edit_input(&view, "title-translated", draft.title.translated);

    application_controller_action(&controller, APP_HOME, 0);
    int can_resume = strstr(test_document(), "Resume draft") != nullptr;
    application_controller_action(&controller, APP_RESUME_DRAFT, 0);

    EXPECT_TRUE(can_resume);
    EXPECT_NE(strstr(test_document(), "Unsaved draft"), nullptr);
    EXPECT_EQ(state.has_unexported_changes, 1);
}

TEST_F(Given_LyricsEditor, When_ReplacementImportIsCancelledFromHome_Then_UnsavedDraftSurvives)
{
    open_draft(&controller);
    strcpy(draft.title.translated, "Keep this draft");
    edit_input(&view, "title-translated", draft.title.translated);
    application_controller_action(&controller, APP_HOME, 0);
    confirm_answer = 0;

    application_controller_import(&controller, embedded_songs[1], strlen(embedded_songs[1]));
    int stayed_home = strstr(test_document(), "id=\"song-index\"") != nullptr;
    application_controller_action(&controller, APP_RESUME_DRAFT, 0);

    EXPECT_EQ(confirm_calls, 1);
    EXPECT_TRUE(stayed_home);
    EXPECT_NE(strstr(test_document(), "Keep this draft"), nullptr);
    EXPECT_EQ(state.has_unexported_changes, 1);
}

TEST_F(Given_LyricsEditor, When_EmptyFileDropped_Then_EditorRemainsHidden)
{
    prepare_drop("empty.yaml", 1, 0);

    browser_view_event(&view, APP_EVENT_DROP);

    EXPECT_EQ(test_drop_reads, 0);
    EXPECT_EQ(strstr(test_document(), "editor-meta"), nullptr);
    EXPECT_STREQ(notice, "Choose a nonempty YAML file no larger than 1 MiB.");
}

TEST_F(Given_LyricsEditor, When_SelectionChanges_Then_MetadataControlsStayMounted)
{
    open_draft(&controller);
    Node title = dom_find("#title");

    application_controller_action(&controller, APP_SELECT_LINE, 2);
    application_controller_action(&controller, APP_CLOSE_PANEL, 0);

    EXPECT_EQ(dom_find("#title"), title);
    EXPECT_STREQ(test_input_value("title"), "Amber");
}

TEST_F(Given_LyricsEditor, When_SearchChanges_Then_SearchControlStaysMounted)
{
    application_controller_action(&controller, APP_HOME, 0);
    Node search = dom_find("#song-search");

    application_controller_search(&controller, "Frost");

    EXPECT_EQ(dom_find("#song-search"), search);
    EXPECT_STREQ(test_input_value("song-search"), "Frost");
    EXPECT_EQ(strstr(test_subtree("#song-index"), "Amber"), nullptr);
}

TEST_F(Given_LyricsEditor, When_ViewRemountedAfterInvalidEdit_Then_InputIsRestoredFromModel)
{
    open_draft(&controller);
    application_controller_link_changed(&controller, 0, 1, "javascript:alert(1)");
    application_controller_action(&controller, APP_APPLY, 0);

    browser_view_detach(&view);
    browser_view_attach(&view, &controller);
    application_state_notify(&state, APPLICATION_RENDER);

    EXPECT_STREQ(test_input_value("link-url-0"), "javascript:alert(1)");
    EXPECT_TRUE(state.pending_changes);
}

TEST_F(Given_LyricsEditor, When_EditorDetaches_Then_ReaderStillRespondsToSelection)
{
    editor_view_destroy(&view.editor_impl);

    application_controller_action(&controller, APP_SELECT_LINE, 2);

    EXPECT_NE(strstr(test_subtree("#panel"), "Sed do eiusmod tempor incididunt."), nullptr);
    EXPECT_EQ(view.editor_impl.subscription.state, nullptr);
    EXPECT_EQ(view.reader_impl.subscription.state, &state);
}

TEST_F(Given_LyricsEditor, When_AnnotationInputChanges_Then_FieldAndMetadataKeepTheirNodes)
{
    open_draft(&controller);
    Node annotation = dom_find("#annotation");
    Node title = dom_find("#title");

    application_controller_changed(&controller, EDITOR_ANNOTATION, "**Live preview**");

    EXPECT_EQ(dom_find("#annotation"), annotation);
    EXPECT_EQ(dom_find("#title"), title);
    EXPECT_STREQ(test_input_value("annotation"), "**Live preview**");
    EXPECT_NE(strstr(test_subtree("#markdown-preview"), "<strong>Live preview</strong>"), nullptr);
}

TEST_F(Given_LyricsEditor, When_TitleSelected_Then_OriginalAndItsNoteAppear)
{
    const char* title = draft.title.original;

    application_controller_action(&controller, APP_SELECT_TITLE, 0);

    EXPECT_NE(strstr(test_subtree("#panel"), title), nullptr);
    EXPECT_NE(strstr(test_subtree("#panel"), "Lorem ipsum dolor sit amet."), nullptr);
    EXPECT_EQ(state.selected_section, TITLE_SECTION);
    EXPECT_STREQ(focus, "panel");
}

TEST_F(Given_LyricsEditor, When_TitlePanelClosed_Then_FocusReturnsToTitle)
{
    application_controller_action(&controller, APP_SELECT_TITLE, 0);

    application_controller_action(&controller, APP_CLOSE_PANEL, 0);

    EXPECT_STREQ(focus, "title-line");
    EXPECT_EQ(dom_find("#panel"), 0);
}

TEST_F(Given_LyricsEditor, When_TitleSelectedOnMobile_Then_TabRemainsInsideTheSheet)
{
    application_controller_action(&controller, APP_SELECT_TITLE, 0);
    test_set_width(390);
    strcpy(test_key, "Tab");
    test_shift = 0;
    test_active_index = 2;

    browser_view_event(&view, APP_EVENT_KEY);

    EXPECT_EQ(test_active_index, 0);
    EXPECT_EQ(test_prevented, 1);
}

TEST_F(Given_LyricsEditor, When_LinkAddedInEditor_Then_LabelAndUrlReachTheReader)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_ADD_LINK, 0);

    edit_input(&view, "link-label-1", "Website");
    edit_input(&view, "link-url-1", "https://example.com");
    application_controller_action(&controller, APP_APPLY, 0);
    application_controller_action(&controller, APP_TOGGLE_EDIT, 0);

    EXPECT_NE(strstr(test_document(), "href=\"https://example.com\""), nullptr);
    EXPECT_NE(strstr(test_document(), "Website"), nullptr);
    EXPECT_FALSE(state.pending_changes);
}

TEST_F(Given_LyricsEditor, When_SharedKeyTyped_Then_AnnotationInputUsesExistingText)
{
    open_draft(&controller);
    application_controller_action(&controller, APP_SELECT_LINE, 2);

    edit_input(&view, "annotation-ref", "note-1");

    EXPECT_STREQ(test_input_value("annotation"), draft.annotations[0].text);
    EXPECT_STREQ(test_input_value("annotation-ref"), "note-1");
}

TEST_F(Given_LyricsEditor, When_ControllerChangesLink_Then_EditorObservesModelWithoutAnInputEvent)
{
    open_draft(&controller);
    Node input = dom_find("#link-label-0");

    application_controller_link_changed(&controller, 0, 0, "Video");

    EXPECT_EQ(dom_find("#link-label-0"), input);
    EXPECT_STREQ(test_input_value("link-label-0"), "Video");
    EXPECT_STREQ(state.songs[SONG_COUNT]->links[0].label, "Source");
    EXPECT_TRUE(state.pending_changes);
}
