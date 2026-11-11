#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "core/Song.h"
#include "fakes/DomMock.h"
}

static const char valid[] = R"yaml(version: "1"
slug: demo
title:
  original: Demo
  translated: Bản dịch
  annotation: title-note
artist: Walker
translator: Translator
links:
  - label: YouTube
    url: https://example.com/video
  - label: Website
    url: https://example.com
annotations:
  note: |-
    **A note** with *emphasis*
  title-note: |-
    A demo
    with two lines
sections:
  - label: Verse
    original: |-
      First line
      Second line
    translated: |
      Ánh sáng
      Bên thềm
    annotations:
      - start: 1
        end: 2
        ref: note
)yaml";

class Given_SongYaml : public ::testing::Test
{
protected:
    Song song = {};
    char error[256] = {};

    void SetUp() override
    {
        ASSERT_TRUE(song_parse(&song, valid, strlen(valid), error, sizeof(error)));
    }
};

class Given_MarkdownRenderer : public ::testing::Test
{
protected:
    Buffer out = {};

    void TearDown() override
    {
        buffer_free(&out);
    }
};

class Given_UrlValidation : public ::testing::Test
{
};

TEST_F(Given_SongYaml, When_MultilineYaml_Then_UnicodeAndReferencesArePreserved)
{
    const char* source = valid;

    int ok = song_parse(&song, source, strlen(source), error, sizeof(error));

    EXPECT_TRUE(ok);
    EXPECT_STREQ(song.sections[0].translated, "Ánh sáng\nBên thềm\n");
    EXPECT_STREQ(song.title.original, "Demo");
    EXPECT_STREQ(song_title(&song), "Bản dịch");
    EXPECT_STREQ(song_description(&song), "A demo\nwith two lines");
    EXPECT_EQ(song.link_count, 2u);
    EXPECT_STREQ(song.links[1].label, "Website");
    EXPECT_EQ(annotation_for_line(&song, 0, 2), 0);
    EXPECT_EQ(annotation_for_selection(&song, TITLE_SECTION, 1), 1);
}

TEST_F(Given_SongYaml, When_ExportedAndReimported_Then_AllFieldsRoundTrip)
{
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    bool literal = yaml.data && strstr(yaml.data, "original: |-") && strstr(yaml.data, "ref:");
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_EQ(memcmp(&song, &imported, sizeof(song)), 0);
    EXPECT_TRUE(literal);
}

TEST_F(Given_SongYaml, When_QuotesWhitespaceAndEmptyOptionalFields_Then_ExportPreservesThem)
{
    strcpy(song.title.translated, "A: 'quoted' \"title\"");
    strcpy(song.annotations[0].text, "  indented\n\ntrailing\n\n");
    song.translator[0] = 0;
    song.link_count = 0;
    memset(song.links, 0, sizeof(song.links));
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_STREQ(song_title(&imported), song.title.translated);
    EXPECT_STREQ(imported.annotations[0].text, song.annotations[0].text);
    EXPECT_EQ(imported.link_count, 0u);
}

TEST_F(Given_SongYaml, When_NoTranslatedTitle_Then_OriginalIsDisplayed)
{
    song.title.translated[0] = 0;

    const char* title = song_title(&song);

    EXPECT_STREQ(title, "Demo");
}

static size_t annotation_columns(const char* yaml)
{
    const char* line = strstr(yaml, "  note:");
    if (!line)
    {
        return 0;
    }
    line = strchr(line, '\n');
    size_t maximum = 0;
    while (line && *++line && strncmp(line, "  title-note:", 13))
    {
        size_t columns = 0;
        const char* end = strchr(line, '\n');
        if (!end)
        {
            end = line + strlen(line);
        }
        for (const char* p = line; p < end; ++p)
        {
            columns += ((unsigned char)*p & 0xc0) != 0x80;
        }
        if (columns > maximum)
        {
            maximum = columns;
        }
        line = *end ? end : NULL;
    }
    return maximum;
}

TEST_F(Given_SongYaml,
       When_LongUnicodeAnnotationExported_Then_LinesFitConfiguredColumnsAndRoundTrip)
{
    Buffer text = {}, yaml = {};
    Song imported = {};
    for (size_t i = 0; i < 200; ++i)
    {
        buffer_puts(&text, "Ánh đi ");
    }
    buffer_puts(&text, "đi");
    strcpy(song.annotations[0].text, text.data);

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    size_t columns = annotation_columns(yaml.data ? yaml.data : "");
    bool folded = yaml.data && strstr(yaml.data, "  note: >-");
    buffer_free(&text);
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_TRUE(folded);
    EXPECT_GT(columns, 0u);
    EXPECT_LE(columns, (size_t)(ONLYC_TEST_YAML_LINE_WIDTH < 7 ? 7 : ONLYC_TEST_YAML_LINE_WIDTH));
    EXPECT_STREQ(imported.annotations[0].text, song.annotations[0].text);
}

TEST_F(Given_SongYaml, When_MarkdownStructureExported_Then_ParagraphsAndBreaksRoundTripExactly)
{
    strcpy(song.annotations[0].text,
           "# Heading\n\n**Bold** and [a link](https://example.com).\\\nNext line\n\n"
           "- First item\n- Second item\n\n> A quote\n\n```text\n"
           "some words  with doubled spaces\n```\n\n");
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_STREQ(imported.annotations[0].text, song.annotations[0].text);
}

TEST_F(Given_SongYaml, When_IndentedMarkdownExported_Then_CodeAndNestedListsStayLiteral)
{
    strcpy(song.annotations[0].text,
           "- Parent\n  - Nested item\n\n    Code with indentation and many words that must never "
           "be folded into a different set of lines when exported as readable YAML.\n");
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    bool literal = yaml.data && strstr(yaml.data, "  note: |");
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_TRUE(literal);
    EXPECT_STREQ(imported.annotations[0].text, song.annotations[0].text);
}

TEST_F(Given_SongYaml, When_LongUnbrokenLinkExported_Then_UrlIsNeverSplit)
{
    strcpy(song.annotations[0].text, "[Link](https://example.com/");
    size_t prefix = strlen(song.annotations[0].text);
    memset(song.annotations[0].text + prefix, 'a', 500);
    strcpy(song.annotations[0].text + prefix + 500, ") followed by text.");
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_STREQ(imported.annotations[0].text, song.annotations[0].text);
}

TEST_F(Given_SongYaml, When_LineEndWhitespaceExported_Then_TextIsNormalizedWithoutEditingTheSource)
{
    strcpy(song.annotations[0].text, "Ánh sáng \t\r\n \t\r\nNext line\t \r\n");
    strcpy(song.sections[0].original, "First line \t\r\nSecond line\t ");
    strcpy(song.sections[0].translated, "Ánh sáng \t\r\nBên thềm\t ");
    Song before = song, imported = {};
    Buffer yaml = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    bool trailing = yaml.data && (strstr(yaml.data, " \n") || strstr(yaml.data, "\t\n"));
    bool carriage_return = yaml.data && strchr(yaml.data, '\r');
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_FALSE(trailing);
    EXPECT_FALSE(carriage_return);
    EXPECT_STREQ(imported.annotations[0].text, "Ánh sáng\n\nNext line\n");
    EXPECT_STREQ(imported.sections[0].original, "First line\nSecond line");
    EXPECT_STREQ(imported.sections[0].translated, "Ánh sáng\nBên thềm");
    EXPECT_EQ(memcmp(&song, &before, sizeof(song)), 0);
}

TEST_F(Given_SongYaml, When_WhitespaceOnlyAnnotationExported_Then_ExportFails)
{
    strcpy(song.annotations[0].text, " \t");
    Buffer yaml = {};

    int exported = song_export(&song, &yaml);
    buffer_free(&yaml);

    EXPECT_FALSE(exported);
}

TEST_F(Given_SongYaml, When_NormalizedYamlExportedAgain_Then_OutputIsIdentical)
{
    strcpy(song.annotations[0].text,
           "A long paragraph with enough words to cross the configured line boundary when "
           "exporting annotations, followed by another sentence for wrapping.  \n\nNext.\t\n");
    Buffer first = {}, second = {};
    Song imported = {};

    int exported = song_export(&song, &first);
    int parsed =
        song_parse(&imported, first.data ? first.data : "", first.len, error, sizeof(error));
    int exported_again = song_export(&imported, &second);
    bool identical = first.data && second.data && !strcmp(first.data, second.data);
    buffer_free(&first);
    buffer_free(&second);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_TRUE(exported_again);
    EXPECT_TRUE(identical);
}

TEST_F(Given_SongYaml, When_InvalidImport_Then_ExistingSongIsUntouched)
{
    Song before = song;

    int ok = song_parse(&song, "invalid: [", 10, error, sizeof(error));

    EXPECT_FALSE(ok);
    EXPECT_EQ(memcmp(&song, &before, sizeof(song)), 0);
}

TEST_F(Given_SongYaml, When_InputExceedsLimit_Then_RejectedBeforeReading)
{
    const char* tiny = "x";

    int ok = song_parse(&song, tiny, MAX_IMPORT + 1, error, sizeof(error));

    EXPECT_FALSE(ok);
    EXPECT_STREQ(error, "Choose a nonempty YAML file no larger than 1 MiB.");
}

TEST_F(Given_SongYaml, When_MultipleDocuments_Then_ImportFails)
{
    Buffer input = {};
    buffer_puts(&input, valid);
    buffer_puts(&input, "---\n");
    buffer_puts(&input, valid);

    int ok = song_parse(&song, input.data, input.len, error, sizeof(error));
    buffer_free(&input);

    EXPECT_FALSE(ok);
}

TEST_F(Given_SongYaml, When_ArtworkAndColorExported_Then_ExternalMetadataRoundTrips)
{
    strcpy(song.artwork, "https://example.com/cover.jpg?revision=1&size=512");
    strcpy(song.color, "#aB1234");
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_STREQ(imported.artwork, song.artwork);
    EXPECT_STREQ(imported.color, song.color);
}

TEST_F(Given_SongYaml, When_TitleTooLong_Then_ImportFailsWithoutTruncating)
{
    Buffer input = {};
    buffer_puts(&input, "version: 1\nslug: demo\nartist: Walker\ntitle: {original: '");
    for (size_t i = 0; i < sizeof(song.title.original); ++i)
    {
        buffer_puts(&input, "x");
    }
    buffer_puts(&input, "'}\nsections: [{label: Verse, original: One, translated: Unus}]\n");

    int ok = song_parse(&song, input.data, input.len, error, sizeof(error));
    buffer_free(&input);

    EXPECT_FALSE(ok);
    EXPECT_STREQ(error, "Field is too long or is not a text scalar.");
}

class Given_InvalidSongYaml : public ::testing::TestWithParam<const char*>
{
};

TEST_P(Given_InvalidSongYaml, When_InvalidSchema_Then_ImportFails)
{
    Song song = {};
    char error[256] = {};
    const char* input = GetParam();

    int ok = song_parse(&song, input, strlen(input), error, sizeof(error));

    EXPECT_FALSE(ok);
    EXPECT_NE(error[0], 0);
    EXPECT_EQ(song.count, 0u);
}

INSTANTIATE_TEST_SUITE_P(InvalidInputs,
                         Given_InvalidSongYaml,
                         ::testing::Values("",
                                           "[",
                                           "title: \xff",
                                           "sections: &self [*self]",
                                           "[[[[[[[[[[[[[[[[[x]]]]]]]]]]]]]]]]]",
                                           "version: 2\ntitle: {original: Demo}",
                                           "version: 1\ntitle: OldScalarTitle",
                                           "version: 1\ntitle: {original: \"a\\0b\"}",
                                           "version: 1\nversion: 1",
                                           "version: 1\nlanguage: Latin",
                                           "version: 1\nyoutube: https://example.com",
                                           "version: 1\ndescription: old"));

class Given_SongMetadata : public Given_SongYaml, public ::testing::WithParamInterface<int>
{
};

TEST_P(Given_SongMetadata, When_InvalidMetadata_Then_ValidationFails)
{
    switch (GetParam())
    {
    case 0:
        song.slug[0] = 0;
        break;
    case 1:
        strcpy(song.slug, "../escape");
        break;
    case 2:
        song.title.original[0] = 0;
        break;
    case 3:
        strcpy(song.title.translated, "Two\nlines");
        break;
    case 4:
        song.artist[0] = 0;
        break;
    case 5:
        strcpy(song.links[0].url, "javascript:alert(1)");
        break;
    case 6:
        song.links[0].label[0] = 0;
        break;
    case 7:
        song.link_count = MAX_LINKS + 1;
        break;
    case 8:
        strcpy(song.artwork, "data:image/png;base64,abc");
        break;
    case 9:
        strcpy(song.color, "#abc");
        break;
    case 10:
        strcpy(song.title.annotation, "missing");
        break;
    case 11:
        song.count = 0;
        break;
    case 12:
        song.count = MAX_SECTIONS + 1;
        break;
    case 13:
        song.sections[0].label[0] = 0;
        break;
    }

    int ok = song_validate(&song, error, sizeof(error));

    EXPECT_FALSE(ok);
    EXPECT_NE(error[0], 0);
}

INSTANTIATE_TEST_SUITE_P(InvalidMetadata, Given_SongMetadata, ::testing::Range(0, 14));

TEST_F(Given_MarkdownRenderer, When_CommonMarkdown_Then_SemanticHtmlIsRendered)
{
    const char* text = "**bold** and *italic*\n\n> quote\n\n- one\n- two\n\n`code`";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_NE(strstr(out.data, "<strong>bold</strong>"), nullptr);
    EXPECT_NE(strstr(out.data, "<em>italic</em>"), nullptr);
    EXPECT_NE(strstr(out.data, "<blockquote>"), nullptr);
    EXPECT_NE(strstr(out.data, "<ul>"), nullptr);
    EXPECT_NE(strstr(out.data, "<code>code</code>"), nullptr);
}

TEST_F(Given_MarkdownRenderer, When_RawHtml_Then_MarkupIsEscaped)
{
    const char* text = "<script>alert(1)</script>\n\n<img src=x onerror=alert(1)>";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_EQ(strstr(out.data, "<script"), nullptr);
    EXPECT_EQ(strstr(out.data, "<img"), nullptr);
    EXPECT_NE(strstr(out.data, "&lt;script&gt;"), nullptr);
}

TEST_F(Given_MarkdownRenderer, When_UnsafeLinks_Then_NoExecutableHrefIsEmitted)
{
    const char* text = "[one](javascript:alert) [two](data:text/html,test) "
                       "[three](java&#x73;cript:alert) ![alt](https://example.com/image.png)";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_EQ(strstr(out.data, "href="), nullptr);
    EXPECT_EQ(strstr(out.data, "<img"), nullptr);
    EXPECT_NE(strstr(out.data, "<a>one</a>"), nullptr);
}

TEST_F(Given_MarkdownRenderer, When_HttpsLink_Then_EscapedSafeLinkIsEmitted)
{
    const char* text = "[source](https://example.com/?a=1&b=2)";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_NE(strstr(out.data, "href=\"https://example.com/?a=1&amp;b=2\""), nullptr);
    EXPECT_NE(strstr(out.data, "rel=\"noopener noreferrer\""), nullptr);
}

TEST_F(Given_MarkdownRenderer, When_EntitiesContainMarkup_Then_TheyRemainTextEntities)
{
    const char* text = "&lt;script&gt; &amp; &#60;img&#62;";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_EQ(strstr(out.data, "<script"), nullptr);
    EXPECT_EQ(strstr(out.data, "<img"), nullptr);
    EXPECT_NE(strstr(out.data, "&lt;script&gt;"), nullptr);
}

TEST_F(Given_MarkdownRenderer, When_EmptyAnnotation_Then_RenderSucceeds)
{
    const char* text = "";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_EQ(out.len, 0u);
}

TEST_F(Given_MarkdownRenderer, When_LinkContainsEntities_Then_DomHrefContainsDecodedCharacters)
{
    const char* text = "[source](https://example.com/?a=1&amp;b=2)";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_NE(strstr(out.data, "href=\"https://example.com/?a=1&amp;b=2\""), nullptr);
    EXPECT_EQ(strstr(out.data, "&amp;amp;"), nullptr);
}

TEST_F(Given_MarkdownRenderer, When_NumericEntities_Then_UnicodeIsDecodedAndInvalidValuesReplaced)
{
    const char* text = "&#x1F30C; &#127756; &#xD800; &#9999999;";

    int ok = markdown_render(text, &out);

    EXPECT_TRUE(ok);
    EXPECT_NE(strstr(out.data, "🌌 🌌 � �"), nullptr);
}

TEST_F(Given_UrlValidation, When_ValidHttpUrls_Then_TheyAreAccepted)
{
    const char *http = "http://example.com/path", *https = "https://example.com/?q=test";

    int first = safe_url(http), second = safe_url(https);

    EXPECT_TRUE(first);
    EXPECT_TRUE(second);
}

TEST_F(Given_UrlValidation, When_ControlsOrMissingHost_Then_TheyAreRejected)
{
    const char *control = "https://example.com/\ntext", *host = "https:///path",
               *backslash = "https://example.com\\evil";

    int first = safe_url(control), second = safe_url(host), third = safe_url(backslash);

    EXPECT_FALSE(first);
    EXPECT_FALSE(second);
    EXPECT_FALSE(third);
}

class Given_LyricLines : public ::testing::Test
{
protected:
    Buffer text = {};

    void TearDown() override
    {
        buffer_free(&text);
    }
};

TEST_F(Given_LyricLines, When_TextEndsWithNewline_Then_NoPhantomLineIsAdded)
{
    const char* lyrics = "First\nSecond\n";

    size_t lines = lyrics_line_count(lyrics);
    lyrics_line_range(lyrics, 2, 2, &text);

    EXPECT_EQ(lines, 2u);
    EXPECT_STREQ(text.data, "Second");
}

TEST_F(Given_LyricLines, When_EmptyText_Then_OneEditableLineRemains)
{
    const char* lyrics = "";

    size_t lines = lyrics_line_count(lyrics);
    lyrics_line_range(lyrics, 1, 1, &text);

    EXPECT_EQ(lines, 1u);
    EXPECT_EQ(text.len, 0u);
}

TEST_F(Given_LyricLines, When_RangeContainsBlankLinesAndUnicode_Then_WholeLinesArePreserved)
{
    const char* lyrics = "First\r\n\r\nTiếng Việt 🌌\r\nLast";

    size_t lines = lyrics_line_count(lyrics);
    lyrics_line_range(lyrics, 2, 3, &text);

    EXPECT_EQ(lines, 4u);
    EXPECT_STREQ(text.data, "\nTiếng Việt 🌌");
}

TEST_F(Given_LyricLines, When_RangeIsReversed_Then_NoTextIsReturned)
{
    const char* lyrics = "First\nSecond";

    lyrics_line_range(lyrics, 2, 1, &text);

    EXPECT_EQ(text.len, 0u);
}

TEST_F(Given_SongYaml,
       When_SharedKeyUsedAcrossVersesAndTitle_Then_AllSelectionsResolveSameDefinition)
{
    song.sections[1] = song.sections[0];
    song.count = 2;
    song.ranges[1] = song.ranges[0];
    song.ranges[1].section = 1;
    song.range_count = 2;
    strcpy(song.title.annotation, "note");

    int ok = song_validate(&song, error, sizeof(error));
    int first = annotation_for_line(&song, 0, 1);
    int second = annotation_for_line(&song, 1, 2);
    int title = annotation_for_selection(&song, TITLE_SECTION, 1);

    EXPECT_TRUE(ok);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, first);
    EXPECT_EQ(title, first);
}

TEST_F(Given_SongYaml, When_SectionDeleted_Then_BindingsMoveButSharedDefinitionsRemain)
{
    song.sections[1] = song.sections[0];
    song.count = 2;
    song.ranges[1] = song.ranges[0];
    song.ranges[1].section = 1;
    song.range_count = 2;

    song_remove_section(&song, 0);
    int ok = song_validate(&song, error, sizeof(error));

    EXPECT_TRUE(ok);
    EXPECT_EQ(song.count, 1u);
    EXPECT_EQ(song.range_count, 1u);
    EXPECT_EQ(song.annotation_count, 2u);
    EXPECT_EQ(song.ranges[0].section, 0u);
    EXPECT_EQ(annotation_for_line(&song, 0, 2), 0);
}

TEST_F(Given_SongYaml, When_BindingDetached_Then_OtherBindingsAndDefinitionSurvive)
{
    song.sections[1] = song.sections[0];
    song.count = 2;
    song.ranges[1] = song.ranges[0];
    song.ranges[1].section = 1;
    song.range_count = 2;

    song_remove_range(&song, 0);

    EXPECT_EQ(annotation_for_line(&song, 0, 1), -1);
    EXPECT_EQ(annotation_for_line(&song, 1, 1), 0);
    EXPECT_EQ(song.annotation_count, 2u);
}

TEST_F(Given_SongYaml, When_DefinitionDeleted_Then_ItsTitleAndVerseReferencesAreRemoved)
{
    strcpy(song.title.annotation, "note");

    song_remove_annotation(&song, 0);
    int ok = song_validate(&song, error, sizeof(error));

    EXPECT_TRUE(ok);
    EXPECT_STREQ(song.title.annotation, "");
    EXPECT_EQ(song.range_count, 0u);
    EXPECT_EQ(song.annotation_count, 1u);
    EXPECT_STREQ(song.annotations[0].key, "title-note");
}

class Given_AnnotationValidation : public Given_SongYaml, public ::testing::WithParamInterface<int>
{
};

TEST_P(Given_AnnotationValidation, When_InvalidBindingOrDefinition_Then_ValidationFails)
{
    switch (GetParam())
    {
    case 0:
        song.ranges[0].first_line = 0;
        break;
    case 1:
        song.ranges[0].first_line = 2;
        song.ranges[0].last_line = 1;
        break;
    case 2:
        song.ranges[0].last_line = 3;
        break;
    case 3:
        song.ranges[0].section = 1;
        break;
    case 4:
        strcpy(song.ranges[0].reference, "missing");
        break;
    case 5:
        song.ranges[1] = song.ranges[0];
        song.range_count = 2;
        break;
    case 6:
        song.annotations[0].text[0] = 0;
        break;
    case 7:
        song.annotations[0].key[0] = 0;
        break;
    case 8:
        strcpy(song.annotations[0].key, "bad key");
        break;
    case 9:
        song.annotations[1] = song.annotations[0];
        break;
    case 10:
        song.annotation_count = MAX_ANNOTATIONS + 1;
        break;
    case 11:
        song.range_count = MAX_ANNOTATION_RANGES + 1;
        break;
    case 12:
        strcpy(song.sections[0].original, "Only one line");
        break;
    }

    int ok = song_validate(&song, error, sizeof(error));

    EXPECT_FALSE(ok);
    EXPECT_NE(error[0], 0);
}

INSTANTIATE_TEST_SUITE_P(InvalidAnnotations, Given_AnnotationValidation, ::testing::Range(0, 13));

class Given_RangeYaml : public ::testing::TestWithParam<const char*>
{
};

TEST_P(Given_RangeYaml, When_InvalidRangeYaml_Then_ImportRejectsIt)
{
    Buffer input = {};
    buffer_puts(&input,
                "version: 1\nslug: demo\ntitle: {original: Demo}\nartist: Walker\n"
                "annotations: {note: Note}\nsections:\n  - label: Verse\n"
                "    original: One\n    translated: Unus\n    annotations: ");
    buffer_puts(&input, GetParam());
    Song song = {};
    char error[256] = {};

    int ok = song_parse(&song, input.data, input.len, error, sizeof(error));
    buffer_free(&input);

    EXPECT_FALSE(ok);
    EXPECT_NE(error[0], 0);
}

INSTANTIATE_TEST_SUITE_P(InvalidRanges,
                         Given_RangeYaml,
                         ::testing::Values("{}",
                                           "[{start: 1, ref: note}]",
                                           "[{start: 1.5, end: 1, ref: note}]",
                                           "[{start: 1, end: 1, ref: missing}]",
                                           "[{start: 1, end: 1, text: OldInlineNote}]",
                                           "[{start: 1, start: 1, end: 1, ref: note}]",
                                           "[{start: 1, end: 1, ref: note, offset: 0}]"));

TEST_F(Given_SongYaml, When_MaximumLinksAndDefinitions_Then_AllEntriesRoundTrip)
{
    song.title.annotation[0] = 0;
    song.range_count = 0;
    memset(song.ranges, 0, sizeof(song.ranges));
    song.annotation_count = MAX_ANNOTATIONS;
    song.link_count = MAX_LINKS;
    for (size_t i = 0; i < MAX_ANNOTATIONS; ++i)
    {
        snprintf(song.annotations[i].key, sizeof(song.annotations[i].key), "note-%zu", i);
        strcpy(song.annotations[i].text, "Note");
    }
    for (size_t i = 0; i < MAX_LINKS; ++i)
    {
        snprintf(song.links[i].label, sizeof(song.links[i].label), "Link %zu", i);
        strcpy(song.links[i].url, "https://example.com");
    }
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_EQ(imported.link_count, (size_t)MAX_LINKS);
    EXPECT_EQ(imported.annotation_count, (size_t)MAX_ANNOTATIONS);
    EXPECT_STREQ(imported.annotations[MAX_ANNOTATIONS - 1].key, "note-127");
}

TEST_F(Given_SongYaml, When_MaximumSectionsAndWholeLineRanges_Then_TheyRoundTrip)
{
    song.count = MAX_SECTIONS;
    for (size_t i = 1; i < MAX_SECTIONS; ++i)
    {
        song.sections[i] = song.sections[0];
    }
    song.range_count = MAX_ANNOTATION_RANGES;
    for (size_t i = 0; i < MAX_SECTIONS; ++i)
    {
        strcpy(song.sections[i].original, "1\n2\n3\n4\n5\n6\n7\n8");
        strcpy(song.sections[i].translated, song.sections[i].original);
        for (size_t line = 0; line < 8; ++line)
        {
            AnnotationRange* range = &song.ranges[8 * i + line];
            range->section = i;
            range->first_line = range->last_line = line + 1;
            strcpy(range->reference, "note");
        }
    }
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_EQ(imported.count, (size_t)MAX_SECTIONS);
    EXPECT_EQ(imported.range_count, (size_t)MAX_ANNOTATION_RANGES);
    EXPECT_EQ(annotation_for_line(&imported, MAX_SECTIONS - 1, 8), 0);
}

TEST_F(Given_SongYaml, When_BindingsAddedOutOfOrder_Then_ExportOrdersRangesWithoutDuplicatingText)
{
    song.ranges[0].first_line = song.ranges[0].last_line = 2;
    song.ranges[1] = song.ranges[0];
    song.ranges[1].first_line = song.ranges[1].last_line = 1;
    song.range_count = 2;
    Buffer yaml = {};
    Song imported = {};

    int exported = song_export(&song, &yaml);
    int parsed = song_parse(&imported, yaml.data ? yaml.data : "", yaml.len, error, sizeof(error));
    buffer_free(&yaml);

    EXPECT_TRUE(exported);
    EXPECT_TRUE(parsed);
    EXPECT_EQ(imported.ranges[0].first_line, 1u);
    EXPECT_EQ(imported.ranges[1].first_line, 2u);
    EXPECT_EQ(imported.annotation_count, 2u);
    EXPECT_EQ(annotation_for_line(&imported, 0, 1), annotation_for_line(&imported, 0, 2));
}

TEST_F(Given_SongYaml, When_LyricsExceedLineLimit_Then_ValidationFails)
{
    Buffer text = {};
    for (size_t i = 0; i <= MAX_LINES; ++i)
    {
        buffer_puts(&text, "x\n");
    }
    strcpy(song.sections[0].original, text.data);
    strcpy(song.sections[0].translated, text.data);

    int ok = song_validate(&song, error, sizeof(error));
    buffer_free(&text);

    EXPECT_FALSE(ok);
    EXPECT_STREQ(error, "At most 256 lines per section are supported.");
}
