#include "platform/web/Browser.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/Artwork.h"
#include "platform/web/Discussion.h"
#include "web_dom.h"

/* Nodes are borrowed by the UI until their subtree is cleared or removed.
 * This C registry owns the corresponding JavaScript references. */
static DomValue* nodes;
static size_t node_count, node_capacity;
static DomValue current_event;

typedef struct Listener
{
    struct Listener* next;
    DomValue target;
    DomValue event;
    BridgeCallback callback;
} Listener;

static Listener* listeners;

static BrowserCallbacks callbacks;

typedef struct
{
    DomValue image;
    BridgeCallback ready;
    BridgeCallback failed;
    unsigned request;
} ArtworkRequest;

static ArtworkRequest* artwork_request;

static void finish_artwork(void)
{
    ArtworkRequest* request = artwork_request;
    if (!request)
    {
        return;
    }
    dom_EventTarget_removeEventListener_2(
        request->image, bridge_string("load"), request->ready.function);
    dom_EventTarget_removeEventListener_2(
        request->image, bridge_string("error"), request->failed.function);
    bridge_callback_destroy(&request->ready);
    bridge_callback_destroy(&request->failed);
    dom_HTMLImageElement_src_set(request->image, bridge_string(""));
    bridge_release(request->image);
    free(request);
    artwork_request = NULL;
}

void browser_set_callbacks(BrowserCallbacks next)
{
    callbacks = next;
}

typedef struct
{
    BridgeCallback callback;
    DomValue timer;
    int fading;
} ToastRequest;

static ToastRequest* toast_request;

static void finish_toast(void)
{
    if (!toast_request)
    {
        return;
    }
    dom_Window_clearTimeout_1(em_Val_global_this(), toast_request->timer);
    bridge_release(toast_request->timer);
    bridge_callback_destroy(&toast_request->callback);
    free(toast_request);
    toast_request = NULL;
}

void ui_shutdown(void)
{
    BridgeScope scope = bridge_scope_begin();
    finish_artwork();
    finish_toast();
    discussion_shutdown();
    while (listeners)
    {
        Listener* listener = listeners;
        listeners = listener->next;
        dom_EventTarget_removeEventListener_2(
            listener->target, listener->event, listener->callback.function);
        bridge_callback_destroy(&listener->callback);
        bridge_release(listener->target);
        bridge_release(listener->event);
        free(listener);
    }
    for (size_t i = 1; i < node_count; ++i)
    {
        bridge_release(nodes[i]);
    }
    node_count = 0;
    bridge_scope_end(scope);
}

static int finish_scope(BridgeScope scope, int result)
{
    bridge_scope_end(scope);
    return result;
}

static DomValue document(void)
{
    return dom_Window_document_get(em_Val_global_this());
}

static void artwork_failed(DomValue arguments, void* context)
{
    (void)arguments;
    ArtworkRequest* request = context;
    unsigned id = request->request;
    finish_artwork();
    if (callbacks.artwork)
    {
        callbacks.artwork(callbacks.context, id, NULL);
    }
}

static void artwork_ready(DomValue arguments, void* context)
{
    (void)arguments;
    ArtworkRequest* request = context;
    DomValue canvas = dom_Document_createElement_1(document(), bridge_string("canvas"));
    dom_HTMLCanvasElement_width_set(canvas, bridge_number(ARTWORK_SAMPLE_SIDE));
    dom_HTMLCanvasElement_height_set(canvas, bridge_number(ARTWORK_SAMPLE_SIDE));
    DomValue settings = bridge_object();
    bridge_set(settings, "colorSpace", bridge_string("srgb"));
    bridge_set(settings, "willReadFrequently", bridge_boolean(true));
    DomValue drawing = dom_HTMLCanvasElement_getContext_2(canvas, bridge_string("2d"), settings);
    if (!bridge_present(drawing) || bridge_is_error(drawing))
    {
        artwork_failed(em_Val_undefined(), request);
        return;
    }
    dom_CanvasRenderingContext2D_imageSmoothingEnabled_set(drawing, bridge_boolean(false));
    DomValue drawn = dom_CanvasRenderingContext2D_drawImage_5(drawing,
                                                              request->image,
                                                              bridge_number(0),
                                                              bridge_number(0),
                                                              bridge_number(ARTWORK_SAMPLE_SIDE),
                                                              bridge_number(ARTWORK_SAMPLE_SIDE));
    if (bridge_is_error(drawn))
    {
        artwork_failed(em_Val_undefined(), request);
        return;
    }
    DomValue pixels =
        dom_CanvasRenderingContext2D_getImageData_4(drawing,
                                                    bridge_number(0),
                                                    bridge_number(0),
                                                    bridge_number(ARTWORK_SAMPLE_SIDE),
                                                    bridge_number(ARTWORK_SAMPLE_SIDE));
    if (bridge_is_error(pixels))
    {
        artwork_failed(em_Val_undefined(), request);
        return;
    }
    DomValue bytes = dom_ImageData_data_get(pixels);
    unsigned char rgba[ARTWORK_SAMPLE_BYTES];
    char color[8];
    if (bridge_copy_bytes(bridge_get(bytes, "buffer"), rgba, sizeof(rgba)) != (int)sizeof(rgba) ||
        !artwork_color(rgba, ARTWORK_SAMPLE_PIXELS, color))
    {
        artwork_failed(em_Val_undefined(), request);
        return;
    }
    unsigned id = request->request;
    finish_artwork();
    if (callbacks.artwork)
    {
        callbacks.artwork(callbacks.context, id, color);
    }
}

void ui_artwork_load(const char* url, unsigned request_id)
{
    BridgeScope scope = bridge_scope_begin();
    finish_artwork();
    if (url && *url)
    {
        ArtworkRequest* request = calloc(1, sizeof(*request));
        if (!request)
        {
            bridge_scope_end(scope);
            return;
        }
        request->request = request_id;
        request->image =
            bridge_retain(dom_Document_createElement_1(document(), bridge_string("img")));
        bridge_callback_init(&request->ready, artwork_ready, request);
        bridge_callback_init(&request->failed, artwork_failed, request);
        artwork_request = request;
        dom_HTMLImageElement_crossOrigin_set(request->image, bridge_string("anonymous"));
        dom_EventTarget_addEventListener_2(
            request->image, bridge_string("load"), request->ready.function);
        dom_EventTarget_addEventListener_2(
            request->image, bridge_string("error"), request->failed.function);
        dom_HTMLImageElement_src_set(request->image, bridge_string(url));
    }
    bridge_scope_end(scope);
}

static DomValue select_one(const char* selector)
{
    return dom_Document_querySelector_1(document(), bridge_string(selector));
}

static DomValue select_all(const char* selector)
{
    return dom_Document_querySelectorAll_1(document(), bridge_string(selector));
}

static DomValue by_id(const char* id)
{
    return dom_Document_getElementById_1(document(), bridge_string(id));
}

static int length(DomValue collection)
{
    return (int)em_Val_as_double(dom_NodeList_length_get(collection));
}

static Node remember(DomValue object)
{
    if (!bridge_present(object))
    {
        return 0;
    }
    size_t free_slot = 0;
    for (size_t i = 1; i < node_count; ++i)
    {
        if (!bridge_present(nodes[i]) && !free_slot)
        {
            free_slot = i;
        }
    }
    if (!free_slot)
    {
        if (node_count == node_capacity)
        {
            size_t capacity = node_capacity ? node_capacity * 2 : 128;
            DomValue* next = realloc(nodes, capacity * sizeof(*next));
            if (!next)
            {
                abort();
            }
            nodes = next;
            node_capacity = capacity;
        }
        if (!node_count)
        {
            nodes[node_count++] = em_Val_null();
        }
        free_slot = node_count++;
    }
    nodes[free_slot] = bridge_retain(object);
    return (Node)free_slot;
}

Node dom_reset(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue root = select_one(selector);
    dom_Element_replaceChildren_0(root);
    for (size_t i = 1; i < node_count; ++i)
    {
        bridge_release(nodes[i]);
    }
    node_count = 0;
    Node node = remember(root);
    return finish_scope(scope, node);
}

Node dom_find(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue object = select_one(selector);
    for (size_t i = 1; bridge_present(object) && i < node_count; ++i)
    {
        if (bridge_present(nodes[i]) && em_Val_seq(nodes[i], object))
        {
            return finish_scope(scope, (Node)i);
        }
    }
    return finish_scope(scope, remember(object));
}

Node dom_create(Node parent, const char* tag)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue node = dom_Document_createElement_1(document(), bridge_string(tag));
    dom_Node_appendChild_1(nodes[parent], node);
    Node handle = remember(node);
    return finish_scope(scope, handle);
}

void dom_text(Node parent, const char* text, int size)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue node = dom_Document_createTextNode_1(document(), bridge_string_n(text, (size_t)size));
    dom_Node_appendChild_1(nodes[parent], node);
    bridge_scope_end(scope);
}

void dom_attr(Node node, const char* name, const char* value)
{
    BridgeScope scope = bridge_scope_begin();
    dom_Element_setAttribute_2(nodes[node], bridge_string(name), bridge_string(value));
    bridge_scope_end(scope);
}

void dom_value(Node node, const char* value)
{
    BridgeScope scope = bridge_scope_begin();
    dom_HTMLInputElement_value_set(nodes[node], bridge_string(value));
    bridge_scope_end(scope);
}

void dom_clear(Node node)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue root = nodes[node];
    for (size_t i = 1; i < node_count; ++i)
    {
        BridgeScope child_scope = bridge_scope_begin();
        if (i != (size_t)node && bridge_present(nodes[i]) &&
            em_Val_as_bool(dom_Node_contains_1(root, nodes[i])))
        {
            bridge_release(nodes[i]);
            nodes[i] = em_Val_null();
        }
        bridge_scope_end(child_scope);
    }
    dom_Element_replaceChildren_0(root);
    bridge_scope_end(scope);
}

void dom_remove(Node node)
{
    BridgeScope scope = bridge_scope_begin();
    dom_clear(node);
    dom_Element_remove_0(nodes[node]);
    bridge_release(nodes[node]);
    nodes[node] = em_Val_null();
    bridge_scope_end(scope);
}

void ui_style(const char* selector, const char* property, const char* value)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue list = select_all(selector);
    DomValue args[] = {bridge_string(property), bridge_string(value)};
    int count = length(list);
    for (int i = 0; i < count; ++i)
    {
        BridgeScope item_scope = bridge_scope_begin();
        dom_CSSStyleDeclaration_setProperty_2(
            dom_HTMLElement_style_get(dom_NodeList_item_1(list, bridge_number(i))),
            args[0],
            args[1]);
        bridge_scope_end(item_scope);
    }
    bridge_scope_end(scope);
}

void ui_clear_styles(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue list = select_all(selector), name = bridge_string("style");
    int count = length(list);
    for (int i = 0; i < count; ++i)
    {
        BridgeScope item_scope = bridge_scope_begin();
        dom_Element_removeAttribute_1(dom_NodeList_item_1(list, bridge_number(i)), name);
        bridge_scope_end(item_scope);
    }
    bridge_scope_end(scope);
}

void ui_attribute(const char* selector, const char* attribute, const char* value)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue list = select_all(selector);
    DomValue args[] = {bridge_string(attribute), bridge_string(value)};
    int count = length(list);
    for (int i = 0; i < count; ++i)
    {
        BridgeScope item_scope = bridge_scope_begin();
        dom_Element_setAttribute_2(dom_NodeList_item_1(list, bridge_number(i)), args[0], args[1]);
        bridge_scope_end(item_scope);
    }
    bridge_scope_end(scope);
}

int ui_width(void)
{
    BridgeScope scope = bridge_scope_begin();
    int width = (int)em_Val_as_double(dom_Window_innerWidth_get(em_Val_global_this()));
    return finish_scope(scope, width);
}

double ui_max_width(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue list = select_all(selector);
    int count = length(list);
    double maximum = 0;
    for (int i = 0; i < count; ++i)
    {
        BridgeScope item_scope = bridge_scope_begin();
        DomValue node = dom_NodeList_item_1(list, bridge_number(i));
        DomValue bounds = dom_Element_getBoundingClientRect_0(node);
        double width = em_Val_as_double(bridge_get(bounds, "width"));
        if (width > maximum)
        {
            maximum = width;
        }
        bridge_scope_end(item_scope);
    }
    bridge_scope_end(scope);
    return maximum;
}

int ui_count(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    int count = length(select_all(selector));
    return finish_scope(scope, count);
}

int ui_read(const char* id, char* text, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    int result = bridge_copy_string(dom_HTMLInputElement_value_get(by_id(id)), text, capacity);
    return finish_scope(scope, result);
}

int ui_path(char* text, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue location = dom_Window_location_get(em_Val_global_this());
    int used = bridge_copy_string(dom_Location_pathname_get(location), text, capacity);
    int tail =
        used < 0
            ? -1
            : bridge_copy_string(dom_Location_hash_get(location), text + used, capacity - used);
    if (tail < 0 && capacity > 0)
    {
        text[0] = '\0';
    }
    return finish_scope(scope, tail < 0 ? -1 : used + tail);
}

void ui_title(const char* text)
{
    BridgeScope scope = bridge_scope_begin();
    dom_Document_title_set(document(), bridge_string(text));
    bridge_scope_end(scope);
}

void ui_route(const char* path)
{
    BridgeScope scope = bridge_scope_begin();
    dom_History_pushState_3(dom_Window_history_get(em_Val_global_this()),
                            em_Val_null(),
                            bridge_string(""),
                            bridge_string(path));
    bridge_scope_end(scope);
}

static void focus(DomValue node)
{
    if (!bridge_present(node))
    {
        return;
    }
    DomValue options = bridge_object();
    dom_FocusOptions_preventScroll_set(options, bridge_boolean(true));
    dom_HTMLElement_focus_1(node, options);
}

void ui_focus(const char* id)
{
    BridgeScope scope = bridge_scope_begin();
    focus(by_id(id));
    bridge_scope_end(scope);
}

void ui_reveal(const char* id)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue node = by_id(id);
    if (bridge_present(node))
    {
        DomValue options = bridge_object();
        bridge_set(options, "block", bridge_string("center"));
        bridge_set(options, "inline", bridge_string("nearest"));
        bridge_set(options, "behavior", bridge_string("instant"));
        dom_Element_scrollIntoView_1(node, options);
    }
    bridge_scope_end(scope);
}

void ui_focus_at(const char* selector, int index)
{
    BridgeScope scope = bridge_scope_begin();
    if (index >= 0)
    {
        focus(bridge_at(select_all(selector), (unsigned)index));
    }
    bridge_scope_end(scope);
}

void ui_set_text(const char* selector, const char* text)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue node = select_one(selector);
    if (bridge_present(node))
    {
        dom_Node_textContent_set(node, bridge_string(text));
    }
    bridge_scope_end(scope);
}

static void expire_toast(DomValue arguments, void* context)
{
    (void)arguments;
    ToastRequest* request = context;
    if (request->fading)
    {
        ui_set_text("#status", "");
        ui_style("#status", "display", "none");
        finish_toast();
        return;
    }
    request->fading = 1;
    ui_attribute("#status", "data-toast-state", "fading");
    ui_style("#status", "opacity", "0");
    bridge_release(request->timer);
    request->timer = bridge_retain(dom_Window_setTimeout_2(
        em_Val_global_this(), request->callback.function, bridge_number(200)));
}

void ui_toast(const char* text)
{
    BridgeScope scope = bridge_scope_begin();
    finish_toast();
    ui_set_text("#status", text);
    ui_style("#status", "display", *text ? "block" : "none");
    ui_attribute("#status", "data-toast-state", "visible");
    ui_style("#status", "opacity", "1");
    if (*text)
    {
        ToastRequest* request = calloc(1, sizeof(*request));
        if (request)
        {
            bridge_callback_init(&request->callback, expire_toast, request);
            request->timer = bridge_retain(dom_Window_setTimeout_2(
                em_Val_global_this(), request->callback.function, bridge_number(3500)));
            toast_request = request;
        }
    }
    bridge_scope_end(scope);
}

int ui_confirm(const char* message)
{
    BridgeScope scope = bridge_scope_begin();
    int result = em_Val_as_bool(dom_Window_confirm_1(em_Val_global_this(), bridge_string(message)));
    return finish_scope(scope, result);
}

void ui_click(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue node = select_one(selector);
    if (bridge_present(node))
    {
        dom_HTMLElement_click_0(node);
    }
    bridge_scope_end(scope);
}

static void dispatch_event(DomValue arguments, void* context)
{
    DomValue previous = current_event;
    current_event = bridge_at(arguments, 0);
    callbacks.event(callbacks.context, (int)(intptr_t)context);
    current_event = previous;
}

void ui_listen(const char* target, const char* event, int code)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue object = !strcmp(target, "window") ? em_Val_global_this() : document();
    Listener* listener = calloc(1, sizeof(*listener));
    if (!listener)
    {
        abort();
    }
    listener->target = bridge_retain(object);
    listener->event = bridge_retain(bridge_string(event));
    bridge_callback_init(&listener->callback, dispatch_event, (void*)(intptr_t)code);
    dom_EventTarget_addEventListener_2(object, listener->event, listener->callback.function);
    listener->next = listeners;
    listeners = listener;
    bridge_scope_end(scope);
}

int ui_event_text(const char* selector, const char* property, char* value, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue target = dom_Event_target_get(current_event);
    DomValue closest = bridge_get(target, "closest");
    DomValue node = bridge_present(closest) ? dom_Element_closest_1(target, bridge_string(selector))
                                            : em_Val_null();
    DomValue attribute = bridge_present(node)
                             ? dom_Element_getAttribute_1(node, bridge_string(property))
                             : em_Val_null();
    int result = bridge_present(attribute) ? bridge_copy_string(attribute, value, capacity) : -1;
    return finish_scope(scope, result);
}

int ui_event_key(char* value, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    int result = bridge_copy_string(dom_KeyboardEvent_key_get(current_event), value, capacity);
    return finish_scope(scope, result);
}

int ui_event_shift(void)
{
    BridgeScope scope = bridge_scope_begin();
    int shift = em_Val_as_bool(dom_MouseEvent_shiftKey_get(current_event));
    return finish_scope(scope, shift);
}

int ui_event_plain_click(void)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue button = dom_MouseEvent_button_get(current_event);
    int plain = bridge_present(button) && em_Val_as_double(button) == 0 &&
                !em_Val_as_bool(dom_MouseEvent_ctrlKey_get(current_event)) &&
                !em_Val_as_bool(dom_MouseEvent_metaKey_get(current_event)) &&
                !em_Val_as_bool(dom_MouseEvent_shiftKey_get(current_event)) &&
                !em_Val_as_bool(dom_MouseEvent_altKey_get(current_event));
    return finish_scope(scope, plain);
}

void ui_prevent_default(void)
{
    BridgeScope scope = bridge_scope_begin();
    if (bridge_present(current_event))
    {
        dom_Event_preventDefault_0(current_event);
        if (bridge_string_equals(dom_Event_type_get(current_event), "beforeunload"))
        {
            dom_BeforeUnloadEvent_returnValue_set(current_event, bridge_string(""));
        }
    }
    bridge_scope_end(scope);
}

int ui_active_matches(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue active = dom_Document_activeElement_get(document());
    int matches = bridge_present(active) &&
                  em_Val_as_bool(dom_Element_matches_1(active, bridge_string(selector)));
    return finish_scope(scope, matches);
}

int ui_active_index(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue active = dom_Document_activeElement_get(document()), list = select_all(selector);
    int count = length(list), result = -1;
    for (int i = 0; i < count; ++i)
    {
        BridgeScope item_scope = bridge_scope_begin();
        int matches = em_Val_seq(active, bridge_at(list, (unsigned)i));
        bridge_scope_end(item_scope);
        if (matches)
        {
            result = i;
            break;
        }
    }
    return finish_scope(scope, result);
}

static DomValue drop_files(void)
{
    return dom_DataTransfer_files_get(dom_DragEvent_dataTransfer_get(current_event));
}

int ui_event_has_files(void)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue types = dom_DataTransfer_types_get(dom_DragEvent_dataTransfer_get(current_event));
    DomValue files = bridge_string("Files");
    int count = (int)em_Val_as_double(bridge_get(types, "length")), found = 0;
    for (int i = 0; i < count; ++i)
    {
        BridgeScope item_scope = bridge_scope_begin();
        found |= em_Val_seq(bridge_at(types, (unsigned)i), files);
        bridge_scope_end(item_scope);
    }
    return finish_scope(scope, found);
}

int ui_drop_count(void)
{
    BridgeScope scope = bridge_scope_begin();
    int count = (int)em_Val_as_double(dom_FileList_length_get(drop_files()));
    return finish_scope(scope, count);
}

int ui_drop_name(char* name, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    int result = bridge_copy_string(
        dom_File_name_get(dom_FileList_item_1(drop_files(), bridge_number(0))), name, capacity);
    return finish_scope(scope, result);
}

/* Clamp before narrowing: files above 2 GiB must not wrap into an allowed size. */
static int file_size(DomValue file)
{
    if (!bridge_present(file))
    {
        return -1;
    }
    double size = em_Val_as_double(dom_Blob_size_get(file));
    return size > INT_MAX ? INT_MAX : size < 0 ? -1 : (int)size;
}

int ui_drop_size(void)
{
    BridgeScope scope = bridge_scope_begin();
    int size = file_size(dom_FileList_item_1(drop_files(), bridge_number(0)));
    return finish_scope(scope, size);
}

int ui_file_size(const char* selector)
{
    BridgeScope scope = bridge_scope_begin();
    int size = file_size(dom_FileList_item_1(dom_HTMLInputElement_files_get(select_one(selector)),
                                             bridge_number(0)));
    return finish_scope(scope, size);
}

typedef struct
{
    unsigned capacity;
    DomValue input;
    BridgeCallback success;
    BridgeCallback failure;
} FileRequest;

static void finish_file(FileRequest* request)
{
    if (bridge_present(request->input))
    {
        dom_HTMLInputElement_value_set(request->input, bridge_string(""));
        bridge_release(request->input);
    }
    bridge_callback_destroy(&request->success);
    bridge_callback_destroy(&request->failure);
    free(request);
}

static void file_failed(DomValue arguments, void* context)
{
    (void)arguments;
    callbacks.file_error(callbacks.context);
    finish_file(context);
}

static void file_ready(DomValue arguments, void* context)
{
    FileRequest* request = context;
    DomValue data = bridge_at(arguments, 0);
    double size = em_Val_as_double(bridge_get(data, "byteLength"));
    if (size < 0 || size > request->capacity)
    {
        file_failed(arguments, context);
        return;
    }
    unsigned count = (unsigned)size;
    char* text = malloc((size_t)count + 1);
    if (!text || bridge_copy_bytes(data, text, count) != (int)count)
    {
        free(text);
        file_failed(arguments, context);
        return;
    }
    text[count] = '\0';
    callbacks.file(callbacks.context, text, count);
    free(text);
    finish_file(request);
}

static void read_file(DomValue file, DomValue input, int capacity)
{
    int size = file_size(file);
    if (capacity < 0 || size < 0 || size > capacity)
    {
        callbacks.file_error(callbacks.context);
        return;
    }
    FileRequest* request = calloc(1, sizeof(*request));
    if (!request)
    {
        callbacks.file_error(callbacks.context);
        return;
    }
    request->capacity = (unsigned)capacity;
    request->input = bridge_retain(input);
    DomValue promise = dom_Blob_arrayBuffer_0(file);
    if (bridge_is_error(promise))
    {
        file_failed(em_Val_undefined(), request);
        return;
    }
    bridge_callback_init(&request->success, file_ready, request);
    bridge_callback_init(&request->failure, file_failed, request);
    DomValue args[] = {request->success.function, request->failure.function};
    /* Promise and ArrayBuffer are ECMAScript objects, outside Web IDL. */
    DomValue result = bridge_call(promise, "then", args, 2, NULL, 0);
    if (bridge_is_error(result))
    {
        file_failed(em_Val_undefined(), request);
    }
}

void ui_drop_read(int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    read_file(dom_FileList_item_1(drop_files(), bridge_number(0)), em_Val_null(), capacity);
    bridge_scope_end(scope);
}

void ui_file_read(const char* selector, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue input = select_one(selector);
    read_file(dom_FileList_item_1(dom_HTMLInputElement_files_get(input), bridge_number(0)),
              input,
              capacity);
    bridge_scope_end(scope);
}

int ui_blob_url(const char* text, unsigned size, const char* mime, char* url, int capacity)
{
    BridgeScope scope = bridge_scope_begin();
    DomValue parts = bridge_construct("Array", NULL, 0, NULL, 0);
    DomValue part = bridge_string_n(text, size);
    bridge_call(parts, "push", &part, 1, NULL, 0);
    DomValue options = bridge_object();
    dom_BlobPropertyBag_type_set(options, bridge_string(mime));
    DomValue blob = dom_Blob_new_2(parts, options);
    DomValue result = dom_URL_createObjectURL_1(blob);
    int count = bridge_copy_string(result, url, capacity);
    if (count < 0)
    {
        dom_URL_revokeObjectURL_1(result);
    }
    return finish_scope(scope, count);
}

typedef struct
{
    BridgeCallback callback;
    char url[];
} RevokeRequest;

static void revoke_url(DomValue arguments, void* context)
{
    (void)arguments;
    RevokeRequest* request = context;
    dom_URL_revokeObjectURL_1(bridge_string(request->url));
    bridge_callback_destroy(&request->callback);
    free(request);
}

void ui_revoke_url(const char* url, int delay_ms)
{
    BridgeScope scope = bridge_scope_begin();
    RevokeRequest* request = calloc(1, sizeof(*request) + strlen(url) + 1);
    if (request)
    {
        strcpy(request->url, url);
        bridge_callback_init(&request->callback, revoke_url, request);
        dom_Window_setTimeout_2(
            em_Val_global_this(), request->callback.function, bridge_number(delay_ms));
    }
    else
    {
        dom_URL_revokeObjectURL_1(bridge_string(url));
    }
    bridge_scope_end(scope);
}
