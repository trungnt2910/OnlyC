#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "TestCatalog.h"
#include "app/model/EditorState.h"
}

class Given_EditorState : public ::testing::Test
{
protected:
    EditorState editor = {};
    Song song = {};

    void SetUp() override
    {
        char error[256];
        ASSERT_TRUE(
            song_parse(&song, embedded_songs[0], strlen(embedded_songs[0]), error, sizeof(error)));
    }

    void TearDown() override
    {
        editor_state_destroy(&editor);
    }
};

TEST_F(Given_EditorState, When_AnnotatedLineLoaded_Then_FieldsContainItsWholeRange)
{
    const Annotation* note = &song.annotations[0];

    int loaded = editor_state_load(&editor, &song, 0, 2);

    EXPECT_TRUE(loaded);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_TITLE), song.title.original);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION), note->text);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION_START), "1");
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION_END), "2");
}

TEST_F(Given_EditorState, When_UnannotatedLineLoaded_Then_EmptyNoteTargetsOnlyThatLine)
{
    ASSERT_TRUE(editor_state_load(&editor, &song, 0, 1));

    int loaded = editor_state_load(&editor, &song, 0, 3);

    EXPECT_TRUE(loaded);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION), "");
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION_START), "3");
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION_END), "3");
}

TEST_F(Given_EditorState, When_InputBufferReused_Then_ModelKeepsItsOwnCopy)
{
    char text[32] = "Uncommitted title";

    int stored = editor_state_set(&editor, EDITOR_TITLE, text);
    strcpy(text, "Reused buffer");

    EXPECT_TRUE(stored);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_TITLE), "Uncommitted title");
    EXPECT_EQ(editor.changed_field, EDITOR_TITLE);
}

TEST_F(Given_EditorState, When_IncompleteRangeEntered_Then_RawInputIsPreserved)
{
    ASSERT_TRUE(editor_state_load(&editor, &song, 0, 1));

    int stored = editor_state_set(&editor, EDITOR_ANNOTATION_START, "");

    EXPECT_TRUE(stored);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION_START), "");
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION_END), "2");
    EXPECT_EQ(song.ranges[0].first_line, 1u);
}

TEST_F(Given_EditorState, When_NoSectionLoaded_Then_PreviousSectionFieldsAreCleared)
{
    ASSERT_TRUE(editor_state_load(&editor, &song, 0, 1));

    int loaded = editor_state_load(&editor, &song, -1, 1);

    EXPECT_TRUE(loaded);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_TITLE), song.title.original);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_LABEL), "");
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_ANNOTATION), "");
}

TEST_F(Given_EditorState, When_UnknownFieldWritten_Then_ExistingInputIsPreserved)
{
    ASSERT_TRUE(editor_state_set(&editor, EDITOR_TITLE, "Draft title"));

    int stored = editor_state_set(&editor, EDITOR_FIELD_COUNT, "Other title");

    EXPECT_FALSE(stored);
    EXPECT_STREQ(editor_state_text(&editor, EDITOR_TITLE), "Draft title");
}
