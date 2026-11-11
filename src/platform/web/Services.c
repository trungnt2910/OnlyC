#include "platform/web/Services.h"

#include "app/ui/Dom.h"
#include "platform/web/Browser.h"

static int location(void* context, char* path, int capacity)
{
    (void)context;
    return ui_path(path, capacity);
}

static void navigate(void* context, const char* path)
{
    (void)context;
    ui_route(path);
}

static int confirm(void* context, const char* question)
{
    (void)context;
    return ui_confirm(question);
}

static int download(void* context, const char* name, const Buffer* data)
{
    (void)context;
    char url[256];
    if (ui_blob_url(
            data->data, (unsigned)data->len, "application/yaml;charset=utf-8", url, sizeof(url)) <
        0)
    {
        return 0;
    }
    Node link = element(dom_find("#app"), "a", NULL, NULL);
    dom_attr(link, "id", "export-download");
    dom_attr(link, "hidden", "");
    dom_attr(link, "href", url);
    dom_attr(link, "download", name);
    ui_click("#export-download");
    dom_remove(link);
    ui_revoke_url(url, 1000);
    return 1;
}

static void select_file(void* context)
{
    (void)context;
    ui_click("#import-file");
}

static void artwork(void* context, const char* url, unsigned request)
{
    (void)context;
    ui_artwork_load(url, request);
}

ApplicationServices browser_services(void)
{
    return (ApplicationServices){.location = location,
                                 .navigate = navigate,
                                 .confirm = confirm,
                                 .download = download,
                                 .select_file = select_file,
                                 .artwork = artwork};
}
