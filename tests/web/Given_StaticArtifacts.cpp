#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "core/Song.h"
#include "embedded.h"
#include "strings/Resources.h"
}

static int read_artifact(const char* name, Buffer* b)
{
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", SITE_DIR, name);
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
    char chunk[4096];
    size_t size;
    while ((size = fread(chunk, 1, sizeof(chunk), f)))
    {
        buffer_append(b, chunk, size);
    }
    int ok = !ferror(f) && !b->failed;
    fclose(f);
    return ok;
}

class Given_StaticArtifacts : public ::testing::Test
{
protected:
    Buffer first = {}, second = {}, third = {};

    void TearDown() override
    {
        buffer_free(&first);
        buffer_free(&second);
        buffer_free(&third);
    }
};

TEST_F(Given_StaticArtifacts, When_SongAndHomePagesBuilt_Then_BodiesAreIdenticalAndMetadataDiffers)
{
    ASSERT_GT(SONG_COUNT, 0);
    char page[96];
    snprintf(page, sizeof(page), "%s.html", embedded_slugs[0]);
    ASSERT_TRUE(read_artifact(page, &first));
    ASSERT_TRUE(read_artifact("index.html", &second));
    ASSERT_NE(strstr(first.data, "<body>"), nullptr);
    ASSERT_NE(strstr(second.data, "<body>"), nullptr);

    int same = strcmp(strstr(first.data, "<body>"), strstr(second.data, "<body>")) == 0;
    const char* song_head_end = strstr(first.data, "<body>");
    const char* home_head_end = strstr(second.data, "<body>");
    bool different_metadata = song_head_end - first.data != home_head_end - second.data ||
                              memcmp(first.data, second.data, (size_t)(song_head_end - first.data));
    int body_script = strstr(strstr(first.data, "<body>"), "<script") != nullptr;

    EXPECT_TRUE(same);
    EXPECT_TRUE(different_metadata);
    EXPECT_FALSE(body_script);
}

TEST_F(Given_StaticArtifacts, When_CleanSongRouteBuilt_Then_ItMatchesPreviewHtml)
{
    ASSERT_GT(SONG_COUNT, 0);
    char page[96], route[112];
    snprintf(page, sizeof(page), "%s.html", embedded_slugs[0]);
    snprintf(route, sizeof(route), "s/%s/index.html", embedded_slugs[0]);
    ASSERT_TRUE(read_artifact(page, &first));
    ASSERT_TRUE(read_artifact(route, &second));

    int equal = first.len == second.len && memcmp(first.data, second.data, first.len) == 0;

    EXPECT_TRUE(equal);
}

TEST_F(Given_StaticArtifacts, When_WasmBuilt_Then_ArtifactHasWasmVersionOneHeader)
{
    ASSERT_TRUE(read_artifact("app.wasm", &first));
    ASSERT_GE(first.len, 8u);
    const unsigned char magic[] = {0, 'a', 's', 'm', 1, 0, 0, 0};

    int valid = memcmp(first.data, magic, sizeof(magic)) == 0;

    EXPECT_TRUE(valid);
}

TEST_F(Given_StaticArtifacts, When_SiteBuilt_Then_StylesheetsAndHtmlInjectionAreAbsent)
{
    ASSERT_TRUE(read_artifact("index.html", &first));
    ASSERT_TRUE(read_artifact("index.html", &second));

    int stylesheet =
        strstr(first.data, "stylesheet") != nullptr || strstr(first.data, "<style") != nullptr;
    int html_injection = strstr(second.data, "innerHTML") != nullptr ||
                         strstr(second.data, "insertAdjacentHTML") != nullptr;
    int style_injection =
        strstr(second.data, "cssText") != nullptr || strstr(second.data, "insertRule") != nullptr;

    EXPECT_FALSE(stylesheet);
    EXPECT_FALSE(html_injection);
    EXPECT_FALSE(style_injection);
}

TEST_F(Given_StaticArtifacts, When_HomePageBuilt_Then_MetadataDescribesTheIndex)
{
    ASSERT_TRUE(read_artifact("index.html", &first));

    buffer_puts(&second, "<title>");
    buffer_html(&second, ONLYC_STRING_CATALOG_TITLE, strlen(ONLYC_STRING_CATALOG_TITLE));
    buffer_html(&second, ONLYC_STRING_TITLE_SEPARATOR, strlen(ONLYC_STRING_TITLE_SEPARATOR));
    buffer_html(&second, ONLYC_STRING_SITE_NAME, strlen(ONLYC_STRING_SITE_NAME));
    buffer_puts(&second, "</title>");
    int title = strstr(first.data, second.data) != nullptr;
    int website = strstr(first.data, "property=\"og:type\" content=\"website\"") != nullptr;
    int root = strstr(first.data, "rel=\"canonical\" href=\"" SITE_ORIGIN "/\"") != nullptr;

    EXPECT_TRUE(title);
    EXPECT_TRUE(website);
    EXPECT_TRUE(root);
}

TEST_F(Given_StaticArtifacts, When_BridgeBuilt_Then_BrowserBindingsRemainInWasm)
{
    ASSERT_TRUE(read_artifact("index.html", &first));
    const char* browser_operations[] = {"querySelector",
                                        "createElement",
                                        "createTextNode",
                                        "setProperty",
                                        "addEventListener",
                                        "arrayBuffer",
                                        "createObjectURL",
                                        "pushState",
                                        "app_event",
                                        "app_import"};

    bool browser_logic = false;
    for (const char* operation : browser_operations)
    {
        browser_logic |= strstr(first.data, operation) != nullptr;
    }

    EXPECT_FALSE(browser_logic);
}
