#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "songpack/PreviewPage.h"
}

class Given_PreviewPage : public testing::Test
{
protected:
    Song song = {};
    Buffer output = {};

    void SetUp() override
    {
        strcpy(song.artist, "Lorem Ipsum");
    }

    void TearDown() override
    {
        buffer_free(&output);
    }
};

TEST_F(Given_PreviewPage, When_MetadataContainsMarkup_Then_ItRemainsTextAndAttributeData)
{
    strcpy(song.slug, "example");
    strcpy(song.title.original, "<img src=x> & Song");
    song.annotation_count = 1;
    strcpy(song.annotations[0].key, "title-note");
    strcpy(song.title.annotation, "title-note");
    strcpy(song.annotations[0].text, "<script>alert(1)</script> & \"quote\"");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool escaped_title = strstr(html, "&lt;img src=x&gt; &amp; Song") != nullptr;
    bool escaped_description =
        strstr(html, "&lt;script&gt;alert(1)&lt;/script&gt; &amp; &quot;quote&quot;") != nullptr;
    bool injected_image = strstr(html, "<img") != nullptr;
    bool injected_script = strstr(html, "<script>alert(1)") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(escaped_title);
    EXPECT_TRUE(escaped_description);
    EXPECT_FALSE(injected_image);
    EXPECT_FALSE(injected_script);
}

TEST_F(Given_PreviewPage, When_MetadataContainsUnicode_Then_TheDocumentPreservesUtf8)
{
    strcpy(song.slug, "example");
    strcpy(song.title.original, "Tiếng Việt 🎵");
    song.annotation_count = 1;
    strcpy(song.annotations[0].key, "title-note");
    strcpy(song.title.annotation, "title-note");
    strcpy(song.annotations[0].text, "Bản dịch có chú thích.");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool title = strstr(html, "content=\"Lorem Ipsum - Tiếng Việt 🎵\"") != nullptr;
    bool description = strstr(html, "Bản dịch có chú thích.") != nullptr;
    bool canonical = strstr(html, "href=\"https://example.test/s/example\"") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(title);
    EXPECT_TRUE(description);
    EXPECT_TRUE(canonical);
}

TEST_F(Given_PreviewPage, When_ExternalArtwork_Then_PreviewLinksImageWithoutEmbeddingIt)
{
    strcpy(song.slug, "example");
    strcpy(song.title.original, "Example");
    strcpy(song.artwork, "https://example.test/cover.jpg?revision=1&size=512");
    strcpy(song.color, "#50a0c8");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool og_image =
        strstr(html,
               "property=\"og:image\" "
               "content=\"https://example.test/cover.jpg?revision=1&amp;size=512\"") != nullptr;
    bool twitter_image = strstr(html, "name=\"twitter:image\"") != nullptr;
    bool large_preview = strstr(html, "content=\"summary_large_image\"") != nullptr;
    bool themed = strstr(html, "name=\"theme-color\" content=\"#0e1c25\"") != nullptr;
    bool image_element = strstr(html, "<img") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(og_image);
    EXPECT_TRUE(twitter_image);
    EXPECT_TRUE(large_preview);
    EXPECT_TRUE(themed);
    EXPECT_FALSE(image_element);
}

TEST_F(Given_PreviewPage,
       When_TitleIsTranslatedAndAnnotated_Then_PreviewIncludesArtistOriginalTranslationAndNote)
{
    strcpy(song.slug, "example");
    strcpy(song.title.original, "Original title");
    strcpy(song.title.translated, "Translated title");
    strcpy(song.title.annotation, "title-note");
    song.annotation_count = 1;
    strcpy(song.annotations[0].key, "title-note");
    strcpy(song.annotations[0].text, "A title explanation.");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool title = strstr(html,
                        "property=\"og:title\" content=\"Lorem Ipsum - Original title "
                        "(Translated title)\"") != nullptr;
    bool twitter = strstr(html,
                          "name=\"twitter:title\" content=\"Lorem Ipsum - Original title "
                          "(Translated title)\"") != nullptr;
    bool description =
        strstr(html, "name=\"description\" content=\"A title explanation.\"") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(title);
    EXPECT_TRUE(twitter);
    EXPECT_TRUE(description);
}

TEST_F(Given_PreviewPage, When_TranslationMatchesOriginal_Then_PreviewOmitsParentheses)
{
    strcpy(song.slug, "amber");
    strcpy(song.title.original, "Amber");
    strcpy(song.title.translated, "Amber");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool title = strstr(html, "property=\"og:title\" content=\"Lorem Ipsum - Amber\"") != nullptr;
    bool twitter =
        strstr(html, "name=\"twitter:title\" content=\"Lorem Ipsum - Amber\"") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(title);
    EXPECT_TRUE(twitter);
}

TEST_F(Given_PreviewPage, When_TranslationIsEmpty_Then_PreviewUsesArtistAndOriginalOnly)
{
    strcpy(song.slug, "amber");
    strcpy(song.title.original, "Amber");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool title = strstr(html, "property=\"og:title\" content=\"Lorem Ipsum - Amber\"") != nullptr;
    bool twitter =
        strstr(html, "name=\"twitter:title\" content=\"Lorem Ipsum - Amber\"") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(title);
    EXPECT_TRUE(twitter);
}

TEST_F(Given_PreviewPage, When_TranslationDiffersOnlyInCase_Then_PreviewIncludesIt)
{
    strcpy(song.slug, "amber");
    strcpy(song.title.original, "Amber");
    strcpy(song.title.translated, "amber");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool title =
        strstr(html, "property=\"og:title\" content=\"Lorem Ipsum - Amber (amber)\"") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(title);
}

TEST_F(Given_PreviewPage, When_ArtistAndTranslationContainMarkup_Then_PreviewEscapesBoth)
{
    strcpy(song.slug, "amber");
    strcpy(song.artist, "Lorem <Ipsum> & \"Dolor\"");
    strcpy(song.title.original, "Amber");
    strcpy(song.title.translated, "<Sit> & \"Amet\"");

    bool rendered = preview_page_render(&song, "https://example.test", &output);
    const char* html = output.data ? output.data : "";
    bool title =
        strstr(html,
               "property=\"og:title\" content=\"Lorem &lt;Ipsum&gt; &amp; &quot;Dolor&quot; "
               "- Amber (&lt;Sit&gt; &amp; &quot;Amet&quot;)\"") != nullptr;
    bool injected_markup = strstr(html, "<Ipsum>") != nullptr || strstr(html, "<Sit>") != nullptr;

    EXPECT_TRUE(rendered);
    EXPECT_TRUE(title);
    EXPECT_FALSE(injected_markup);
}
