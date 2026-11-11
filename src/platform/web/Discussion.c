#include "platform/web/Discussion.h"

#include <string.h>

#include "platform/web/Browser.h"
#include "strings/Resources.h"
#include "web_dom.h"

/* Keep Giscus outside #app so rebuilding a reader cannot destroy its iframe,
 * discard a comment draft, or accumulate copies of its global listeners. */
static struct
{
    DomValue host, container, script, frame, config;
    BridgeCallback script_loaded, frame_loaded, message;
    int pending;
} discussion;

static void attribute(DomValue node, const char* name, const char* value)
{
    dom_Element_setAttribute_2(node, bridge_string(name), bridge_string(value));
}

static void send_config(void)
{
    if (!bridge_present(discussion.frame) || !bridge_present(discussion.config))
    {
        return;
    }
    DomValue update = bridge_object(), message = bridge_object();
    bridge_set(update, "setConfig", discussion.config);
    bridge_set(message, "giscus", update);
    dom_Window_postMessage_2(dom_HTMLIFrameElement_contentWindow_get(discussion.frame),
                             message,
                             bridge_string("https://giscus.app"));
}

static void frame_loaded(DomValue arguments, void* context)
{
    (void)arguments;
    (void)context;
    /* Covers lazy iframe loading and Giscus reloading after sign-out. */
    discussion.pending = 1;
    send_config();
}

static void message_received(DomValue arguments, void* context)
{
    (void)context;
    DomValue event = bridge_at(arguments, 0);
    if (!discussion.pending || !bridge_present(discussion.frame) ||
        !bridge_string_equals(dom_MessageEvent_origin_get(event), "https://giscus.app") ||
        !em_Val_seq(dom_MessageEvent_source_get(event),
                    dom_HTMLIFrameElement_contentWindow_get(discussion.frame)))
    {
        return;
    }
    DomValue data = bridge_get(dom_MessageEvent_data_get(event), "giscus");
    if (em_Val_as_bool(bridge_get(data, "resizeHeight")))
    {
        /* A resize confirms hydration. A load event alone can precede the
         * widget installing its configuration listener. Retry once, then stop. */
        discussion.pending = 0;
        send_config();
    }
}

static void script_loaded(DomValue arguments, void* context)
{
    (void)arguments;
    (void)context;
    discussion.frame = bridge_retain(
        dom_Element_querySelector_1(discussion.container, bridge_string("iframe.giscus-frame")));
    if (bridge_present(discussion.frame))
    {
        bridge_callback_init(&discussion.frame_loaded, frame_loaded, NULL);
        dom_EventTarget_addEventListener_2(
            discussion.frame, bridge_string("load"), discussion.frame_loaded.function);
        send_config();
    }
}

static void create_widget(void)
{
    DomValue document = dom_Window_document_get(em_Val_global_this());
    discussion.host =
        bridge_retain(dom_Document_createElement_1(document, bridge_string("section")));
    attribute(discussion.host, "id", "song-discussion");
    attribute(discussion.host, "class", "song-discussion");
    attribute(discussion.host, "aria-label", ONLYC_STRING_COMMENTS_LABEL);
    discussion.container =
        bridge_retain(dom_Document_createElement_1(document, bridge_string("div")));
    attribute(discussion.container, "id", "comments");
    attribute(discussion.container, "class", "giscus");
    dom_Node_appendChild_1(discussion.host, discussion.container);
    dom_Node_appendChild_1(dom_Document_body_get(document), discussion.host);
    discussion.script =
        bridge_retain(dom_Document_createElement_1(document, bridge_string("script")));
    dom_HTMLScriptElement_src_set(discussion.script, bridge_string("https://giscus.app/client.js"));
    dom_HTMLScriptElement_async_set(discussion.script, bridge_boolean(true));
    attribute(discussion.script, "crossorigin", "anonymous");
    attribute(discussion.script, "data-repo", ONLYC_STRING_GISCUS_REPO);
    attribute(discussion.script, "data-repo-id", ONLYC_STRING_GISCUS_REPO_ID);
    attribute(discussion.script, "data-category", ONLYC_STRING_GISCUS_CATEGORY);
    attribute(discussion.script, "data-category-id", ONLYC_STRING_GISCUS_CATEGORY_ID);
    attribute(discussion.script, "data-mapping", "specific");
    attribute(discussion.script, "data-strict", "1");
    attribute(discussion.script, "data-reactions-enabled", "1");
    attribute(discussion.script, "data-emit-metadata", "0");
    attribute(discussion.script, "data-input-position", "top");
    attribute(discussion.script, "data-theme", ONLYC_STRING_GISCUS_THEME);
    attribute(discussion.script, "data-lang", ONLYC_STRING_GISCUS_LANG);
    attribute(discussion.script, "data-loading", "lazy");
    bridge_callback_init(&discussion.script_loaded, script_loaded, NULL);
    dom_EventTarget_addEventListener_2(
        discussion.script, bridge_string("load"), discussion.script_loaded.function);
    bridge_callback_init(&discussion.message, message_received, NULL);
    dom_EventTarget_addEventListener_2(
        em_Val_global_this(), bridge_string("message"), discussion.message.function);
}

void ui_discussion(const char* term, const char* description, const char* backlink)
{
    if (!ONLYC_STRING_GISCUS_REPO[0] || !ONLYC_STRING_GISCUS_REPO_ID[0] ||
        !ONLYC_STRING_GISCUS_CATEGORY_ID[0])
    {
        return;
    }
    BridgeScope scope = bridge_scope_begin();
    if (!term)
    {
        if (bridge_present(discussion.host))
        {
            dom_HTMLElement_hidden_set(discussion.host, bridge_boolean(true));
        }
        bridge_scope_end(scope);
        return;
    }
    int first = !bridge_present(discussion.host);
    if (first)
    {
        create_widget();
    }
    dom_HTMLElement_hidden_set(discussion.host, bridge_boolean(false));
    bridge_release(discussion.config);
    discussion.config = bridge_retain(bridge_object());
    bridge_set(discussion.config, "term", bridge_string(term));
    bridge_set(discussion.config, "description", bridge_string(description));
    bridge_set(discussion.config, "backLink", bridge_string(backlink));
    bridge_set(discussion.config, "strict", bridge_boolean(true));
    discussion.pending = 1;
    /* The official script may still be downloading when the song changes. */
    attribute(discussion.script, "data-term", term);
    if (first)
    {
        dom_Node_appendChild_1(discussion.host, discussion.script);
    }
    send_config();
    bridge_scope_end(scope);
}

void discussion_shutdown(void)
{
    if (!bridge_present(discussion.host))
    {
        return;
    }
    BridgeScope scope = bridge_scope_begin();
    dom_EventTarget_removeEventListener_2(
        discussion.script, bridge_string("load"), discussion.script_loaded.function);
    bridge_callback_destroy(&discussion.script_loaded);
    dom_EventTarget_removeEventListener_2(
        em_Val_global_this(), bridge_string("message"), discussion.message.function);
    bridge_callback_destroy(&discussion.message);
    if (bridge_present(discussion.frame))
    {
        dom_EventTarget_removeEventListener_2(
            discussion.frame, bridge_string("load"), discussion.frame_loaded.function);
        bridge_callback_destroy(&discussion.frame_loaded);
    }
    dom_Element_remove_0(discussion.host);
    bridge_release(discussion.host);
    bridge_release(discussion.container);
    bridge_release(discussion.script);
    bridge_release(discussion.frame);
    bridge_release(discussion.config);
    memset(&discussion, 0, sizeof(discussion));
    bridge_scope_end(scope);
}
