#include "songpack/PreviewPage.h"

#include <string.h>

#include <libxml/HTMLtree.h>
#include <libxml/tree.h>

#include "core/Color.h"
#include "strings/Resources.h"

/* Browser startup is the only script content. HTML nodes and attributes below
 * are constructed through libxml2; song metadata never enters this script. */
static const char bootstrap[] =
    "\nimport { Emlite } from '/emlite.js';\n"
    "\n"
    "const runtime = new Emlite({ initialPages: 64, maximumPages: 1024 });\n"
    "const { instance } = await WebAssembly.instantiateStreaming(fetch('/app.wasm'), {\n"
    "    env: runtime.env,\n"
    "    wasi_snapshot_preview1: {\n"
    "        fd_close: () => 8,\n"
    "        fd_seek: () => 8,\n"
    "        fd_write: () => 8\n"
    "    }\n"
    "});\n"
    "runtime.setExports(instance.exports);\n"
    "instance.exports.app_start();\n";

typedef struct
{
    xmlDoc* document;
    bool failed;
} PageBuilder;

static xmlNode* element(PageBuilder* page, xmlNode* parent, const char* name, const char* text)
{
    if (page->failed)
    {
        return NULL;
    }
    xmlNode* node = xmlNewTextChild(parent, NULL, BAD_CAST name, BAD_CAST text);
    page->failed = !node;
    return node;
}

static void attribute(PageBuilder* page, xmlNode* node, const char* name, const char* value)
{
    if (!page->failed)
    {
        page->failed = !xmlNewProp(node, BAD_CAST name, BAD_CAST value);
    }
}

static void
metadata(PageBuilder* page, xmlNode* head, const char* kind, const char* name, const char* value)
{
    xmlNode* meta = element(page, head, "meta", NULL);
    attribute(page, meta, kind, name);
    attribute(page, meta, "content", value);
}

static void song_preview_title(const Song* song, Buffer* title)
{
    buffer_puts(title, song->artist);
    buffer_puts(title, " - ");
    buffer_puts(title, song->title.original);
    if (song->title.translated[0] && strcmp(song->title.original, song->title.translated))
    {
        buffer_puts(title, " (");
        buffer_puts(title, song->title.translated);
        buffer_puts(title, ")");
    }
}

static void page_head(PageBuilder* page, xmlNode* root, const Song* song, const char* origin)
{
    const char* title = song ? song_title(song) : ONLYC_STRING_CATALOG_TITLE;
    const char* description = song ? song_description(song) : ONLYC_STRING_SITE_DESCRIPTION;
    Buffer heading = {0}, preview_title = {0}, url = {0};
    buffer_puts(&heading, title);
    buffer_puts(&heading, ONLYC_STRING_TITLE_SEPARATOR);
    buffer_puts(&heading, ONLYC_STRING_SITE_NAME);
    if (song)
    {
        song_preview_title(song, &preview_title);
    }
    else if (!heading.failed)
    {
        buffer_puts(&preview_title, heading.data);
    }
    buffer_puts(&url, origin);
    buffer_puts(&url, song ? "/s/" : "/");
    if (song)
    {
        buffer_puts(&url, song->slug);
    }
    page->failed |= heading.failed || preview_title.failed || url.failed;
    xmlNode* head = element(page, root, "head", NULL);
    xmlNode* charset = element(page, head, "meta", NULL);
    attribute(page, charset, "charset", "utf-8");
    metadata(page, head, "name", "viewport", "width=device-width,initial-scale=1");
    char theme[8] = "#07131e";
    Color seed;
    if (song && color_parse(song->color, &seed))
    {
        color_mix(seed, (Color){.red = 5, .green = 10, .blue = 15}, 12, theme);
    }
    metadata(page, head, "name", "theme-color", theme);
    element(page, head, "title", heading.data);
    metadata(page, head, "name", "description", description);
    metadata(page, head, "property", "og:type", song ? "article" : "website");
    metadata(page, head, "property", "og:site_name", ONLYC_STRING_SITE_NAME);
    metadata(page, head, "property", "og:title", preview_title.data);
    metadata(page, head, "name", "twitter:title", preview_title.data);
    metadata(page, head, "property", "og:description", description);
    metadata(page, head, "property", "og:url", url.data);
    if (song && song->artwork[0])
    {
        metadata(page, head, "property", "og:image", song->artwork);
        metadata(page, head, "property", "og:image:alt", song_title(song));
        metadata(page, head, "name", "twitter:image", song->artwork);
        metadata(page, head, "name", "twitter:image:alt", song_title(song));
    }
    metadata(page,
             head,
             "name",
             "twitter:card",
             song && song->artwork[0] ? "summary_large_image" : "summary");
    xmlNode* canonical = element(page, head, "link", NULL);
    attribute(page, canonical, "rel", "canonical");
    attribute(page, canonical, "href", url.data);
    xmlNode* script = element(page, head, "script", bootstrap);
    attribute(page, script, "type", "module");
    buffer_free(&heading);
    buffer_free(&preview_title);
    buffer_free(&url);
}

static void page_body(PageBuilder* page, xmlNode* root)
{
    xmlNode* body = element(page, root, "body", NULL);
    xmlNode* main = element(page, body, "main", NULL);
    attribute(page, main, "id", "app");
    xmlNode* loading = element(page, main, "p", ONLYC_STRING_LOADING);
    attribute(page, loading, "class", "loading");
    xmlNode* status = element(page, body, "div", NULL);
    attribute(page, status, "id", "status");
    attribute(page, status, "role", "status");
    attribute(page, status, "aria-live", "polite");
    element(page, body, "noscript", ONLYC_STRING_JAVASCRIPT_REQUIRED);
}

bool preview_page_render(const Song* song, const char* origin, Buffer* output)
{
    PageBuilder page = {.document = htmlNewDocNoDtD(NULL, NULL)};
    if (!page.document)
    {
        return false;
    }
    page.document->encoding = xmlStrdup(BAD_CAST "UTF-8");
    xmlNode* root = xmlNewNode(NULL, BAD_CAST "html");
    if (!root || !page.document->encoding)
    {
        xmlFreeNode(root);
        xmlFreeDoc(page.document);
        return false;
    }
    xmlDocSetRootElement(page.document, root);
    page.failed = !xmlCreateIntSubset(page.document, BAD_CAST "html", NULL, NULL);
    attribute(&page, root, "lang", "en");
    page_head(&page, root, song, origin);
    page_body(&page, root);
    xmlChar* serialized = NULL;
    int length = 0;
    if (!page.failed)
    {
        htmlDocDumpMemoryFormat(page.document, &serialized, &length, 0);
        if (serialized && length > 0)
        {
            buffer_append(output, (const char*)serialized, (size_t)length);
        }
        else
        {
            page.failed = true;
        }
    }
    xmlFree(serialized);
    xmlFreeDoc(page.document);
    return !page.failed && !output->failed;
}
