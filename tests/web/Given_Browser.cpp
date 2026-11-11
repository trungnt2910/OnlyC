#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>

extern "C"
{
#include "app/view/ApplicationEvents.h"
#include "interop/Bridge.h"
#include "platform/web/Browser.h"
#include "strings/Resources.h"
}

/* A value/handle test double. Browser semantics are exercised separately in
 * Chrome; this mock targets C ownership, callback lifetimes and bounds checks. */
typedef struct Value Value;
typedef Value* (*Method)(Value* receiver, Value* const* arguments, unsigned count);

struct Value
{
    char* text;
    double number;
    Method method;
    Callback callback;
    Handle context;
    bool numeric;
    Handle arguments[16];
    unsigned argument_count;

    struct
    {
        char name[48];
        Value* value;
    } properties[48];

    unsigned property_count;
};

static Value *objects[4096], *handles[4096];
static unsigned object_count, handle_count;
static unsigned references[4096];
static int live_handles, invalid_releases;
static int imported_count, file_errors, event_count;
static char imported_text[128];
static void (*on_event)(int code);
static Value *resolve_file, *reject_file, *listener;

static Value* value(void)
{
    Value* result = (Value*)calloc(1, sizeof(Value));
    objects[object_count++] = result;
    return result;
}

static Value* number(double number_value)
{
    Value* result = value();
    result->number = number_value;
    result->numeric = true;
    return result;
}

static Value* string(const char* text)
{
    Value* result = value();
    result->text = (char*)malloc(strlen(text) + 1);
    strcpy(result->text, text);
    return result;
}

static void property(Value* object, const char* key, Value* content)
{
    unsigned index = 0;
    while (index < object->property_count && strcmp(object->properties[index].name, key))
    {
        ++index;
    }
    if (index == object->property_count)
    {
        ++object->property_count;
    }
    snprintf(object->properties[index].name, sizeof(object->properties[index].name), "%s", key);
    object->properties[index].value = content;
}

static Value* get(Value* object, const char* key)
{
    if (!object)
    {
        return nullptr;
    }
    for (unsigned i = 0; i < object->property_count; ++i)
    {
        if (!strcmp(object->properties[i].name, key))
        {
            return object->properties[i].value;
        }
    }
    return nullptr;
}

static void method(Value* object, const char* key, Method implementation)
{
    Value* function = value();
    function->method = implementation;
    property(object, key, function);
}

static Handle hold(Value* object)
{
    if (!object)
    {
        return EMLITE_NULL;
    }
    ++live_handles;
    handles[++handle_count] = object;
    references[handle_count] = 1;
    return handle_count;
}

static Value* collection(Value* first)
{
    Value* result = value();
    property(result, "0", first);
    property(result, "length", number(first ? 1 : 0));
    return result;
}

static void invoke(Value* callback, Value* argument)
{
    Handle arguments = hold(collection(argument));
    callback->callback(arguments, callback->context);
}

static Value* copy_to_heap(Value*, Value* const* args, unsigned)
{
    memcpy((void*)(uintptr_t)args[1]->number, args[0]->text, strlen(args[0]->text));
    return nullptr;
}

extern "C"
{
    void emlite_init_handle_table(void)
    {
    }

    em_Val em_Val_from_handle(Handle handle)
    {
        return {handle};
    }

    em_Val em_Val_null(void)
    {
        return {EMLITE_NULL};
    }

    em_Val em_Val_undefined(void)
    {
        return {EMLITE_UNDEFINED};
    }

    em_Val em_Val_global_this(void)
    {
        return {EMLITE_GLOBALTHIS};
    }

    em_Val em_Val_from_double(double n)
    {
        return {hold(number(n))};
    }

    em_Val em_Val_from_uint(unsigned n)
    {
        return {hold(number(n))};
    }

    em_Val em_Val_from_bool(bool n)
    {
        return {n ? EMLITE_TRUE : EMLITE_FALSE};
    }

    em_Val em_Val_object(void)
    {
        return {hold(value())};
    }

    em_Val em_Val_from_string(const char* s)
    {
        return {hold(string(s))};
    }

    Handle emlite_val_make_str(const char* text, size_t length)
    {
        Value* result = value();
        result->text = (char*)calloc(length + 1, 1);
        memcpy(result->text, text, length);
        return hold(result);
    }

    em_Val em_Val_get(em_Val object, em_Val key)
    {
        Value* property_key = handles[key.h];
        char index[32];
        snprintf(index, sizeof(index), "%u", (unsigned)property_key->number);
        return {hold(get(handles[object.h], property_key->text ? property_key->text : index))};
    }

    void em_Val_set(em_Val object, em_Val key, em_Val input)
    {
        property(handles[object.h], handles[key.h]->text, handles[input.h]);
    }

    Handle emlite_val_new_array(void)
    {
        return hold(value());
    }

    void emlite_val_push(Handle array, Handle argument)
    {
        Value* list = handles[array];
        list->arguments[list->argument_count++] = argument;
    }

    Handle emlite_val_obj_call(Handle object, const char* name, size_t, Handle array)
    {
        Value* args[16] = {};
        Value* list = handles[array];
        for (unsigned i = 0; i < list->argument_count; ++i)
        {
            args[i] = handles[list->arguments[i]];
        }
        // Web IDL indexed collection access uses item() rather than named properties.
        if (!strcmp(name, "item"))
        {
            char index[32];
            snprintf(index, sizeof(index), "%u", (unsigned)args[0]->number);
            return hold(get(handles[object], index));
        }
        Value* function = get(handles[object], name);
        return function ? hold(function->method(handles[object], args, list->argument_count))
                        : EMLITE_UNDEFINED;
    }

    Handle emlite_val_construct_new(Handle constructor, Handle array)
    {
        Value* list = handles[array];
        if (handles[constructor] && handles[constructor]->text &&
            !strcmp(handles[constructor]->text, "Uint8Array"))
        {
            return hold(handles[list->arguments[0]]);
        }
        return hold(value());
    }

    em_Val em_Val_make_fn(Callback callback, Handle context)
    {
        Value* function = value();
        function->callback = callback;
        function->context = context;
        return {hold(function)};
    }

    char* em_Val_as_string(em_Val input)
    {
        const char* source = handles[input.h] ? handles[input.h]->text : nullptr;
        if (!source || !*source)
        {
            return nullptr;
        }
        char* copy = (char*)malloc(strlen(source) + 1);
        strcpy(copy, source);
        return copy;
    }

    double em_Val_as_double(em_Val input)
    {
        return handles[input.h] ? handles[input.h]->number : 0;
    }

    bool em_Val_as_bool(em_Val input)
    {
        Value* object = handles[input.h];
        return object && (object->text ? !!*object->text : !object->numeric || object->number != 0);
    }

    bool em_Val_is_string(em_Val input)
    {
        return handles[input.h] && handles[input.h]->text;
    }

    bool em_Val_instanceof(em_Val, em_Val)
    {
        return false;
    }

    bool em_Val_seq(em_Val left, em_Val right)
    {
        Value *a = handles[left.h], *b = handles[right.h];
        return a == b || (a && b && a->text && b->text && !strcmp(a->text, b->text));
    }

    void emlite_val_inc_ref(Handle handle)
    {
        if (handle > EMLITE_RESERVED)
        {
            ++references[handle];
        }
    }

    void emlite_val_dec_ref(Handle handle)
    {
        if (handle <= EMLITE_RESERVED)
        {
            return;
        }
        if (!references[handle])
        {
            ++invalid_releases;
        }
        else if (!--references[handle])
        {
            handles[handle] = nullptr;
            --live_handles;
        }
    }

    void em_Val_delete(em_Val input)
    {
        emlite_val_dec_ref(input.h);
    }

    void fake_file(void*, const char* text, size_t length)
    {
        ++imported_count;
        snprintf(imported_text, sizeof(imported_text), "%.*s", (int)length, text);
    }

    void fake_file_error(void*)
    {
        ++file_errors;
    }

    void fake_event(void*, int code)
    {
        ++event_count;
        if (on_event)
        {
            on_event(code);
        }
    }
}

class Given_EmliteBridge : public ::testing::Test
{
protected:
    BridgeScope scope;

    void SetUp() override
    {
        browser_set_callbacks(BrowserCallbacks{.context = nullptr,
                                               .event = fake_event,
                                               .file = fake_file,
                                               .file_error = fake_file_error,
                                               .artwork = nullptr});
        scope = bridge_scope_begin();
        object_count = 0;
        handle_count = EMLITE_RESERVED;
        memset(references, 0, sizeof(references));
        live_handles = invalid_releases = 0;
        imported_count = file_errors = event_count = 0;
        imported_text[0] = '\0';
        memset(handles, 0, sizeof(handles));
        handles[EMLITE_GLOBALTHIS] = value();
        handles[EMLITE_FALSE] = number(0);
        handles[EMLITE_TRUE] = number(1);
        property(handles[EMLITE_GLOBALTHIS], "Uint8Array", string("Uint8Array"));
        Value* heap = value();
        method(heap, "set", copy_to_heap);
        property(handles[EMLITE_GLOBALTHIS], "HEAPU8", heap);
        resolve_file = reject_file = listener = nullptr;
        on_event = nullptr;
    }

    void TearDown() override
    {
        ui_shutdown();
        bridge_scope_end(scope);
        for (unsigned i = 0; i < object_count; ++i)
        {
            free(objects[i]->text);
            free(objects[i]);
        }
    }
};

TEST_F(Given_EmliteBridge, When_NestedScopeEnds_Then_OuterReferencesRemainAlive)
{
    DomValue outer = bridge_string("outer");
    BridgeScope inner = bridge_scope_begin();
    bridge_string("inner");

    bridge_scope_end(inner);
    char text[16];
    int copied = bridge_copy_string(outer, text, sizeof(text));

    EXPECT_EQ(copied, 5);
    EXPECT_STREQ(text, "outer");
    EXPECT_EQ(live_handles, 1);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_EmliteBridge, When_ReferenceRetained_Then_ItOutlivesItsScope)
{
    BridgeScope inner = bridge_scope_begin();
    DomValue owned = bridge_retain(bridge_string("retained"));

    bridge_scope_end(inner);
    char text[16];
    int copied = bridge_copy_string(owned, text, sizeof(text));
    bridge_release(owned);

    EXPECT_EQ(copied, 8);
    EXPECT_STREQ(text, "retained");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

static void allocating_callback(DomValue arguments, void* context)
{
    *(int*)context = (int)em_Val_as_double(bridge_at(arguments, 0));
    bridge_string("temporary callback value");
}

TEST_F(Given_EmliteBridge, When_CallbackReturns_Then_ItsTemporaryReferencesAreReleased)
{
    int result = 0;
    BridgeCallback callback = {};
    bridge_callback_init(&callback, allocating_callback, &result);
    Value* function = handles[callback.function.h];
    bridge_scope_end(scope);

    invoke(function, number(42));
    bridge_callback_destroy(&callback);

    EXPECT_EQ(result, 42);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_EmliteBridge, When_ManyTemporariesAllocated_Then_ScopeReleasesAllOfThem)
{
    BridgeScope inner = bridge_scope_begin();

    for (int i = 0; i < 500; ++i)
    {
        bridge_number(i);
    }
    bridge_scope_end(inner);

    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

/* Browser-binding tests use the same generic value transport. */
class Given_BrowserBindings : public Given_EmliteBridge
{
};

static Value* selected_input;

static Value* return_input(Value*, Value* const*, unsigned)
{
    return selected_input;
}

static Value* element_bounds(Value* receiver, Value* const*, unsigned)
{
    return get(receiver, "bounds");
}

TEST_F(Given_BrowserBindings,
       When_ElementsMeasured_Then_WidestFractionalWidthIsReturnedWithoutLeaks)
{
    Value* document = value();
    property(handles[EMLITE_GLOBALTHIS], "document", document);
    method(document, "querySelectorAll", return_input);
    selected_input = collection(nullptr);
    Value* first = value();
    Value* second = value();
    Value* third = value();
    method(first, "getBoundingClientRect", element_bounds);
    method(second, "getBoundingClientRect", element_bounds);
    method(third, "getBoundingClientRect", element_bounds);
    Value* first_bounds = value();
    Value* second_bounds = value();
    Value* third_bounds = value();
    property(first_bounds, "width", number(315.25));
    property(second_bounds, "width", number(629.875));
    property(third_bounds, "width", number(420.5));
    property(first, "bounds", first_bounds);
    property(second, "bounds", second_bounds);
    property(third, "bounds", third_bounds);
    property(selected_input, "0", first);
    property(selected_input, "1", second);
    property(selected_input, "2", third);
    property(selected_input, "length", number(3));

    double width = ui_max_width(".lyric-line");

    EXPECT_DOUBLE_EQ(width, 629.875);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_NoElementsMatch_Then_MeasuredWidthIsZeroWithoutLeaks)
{
    Value* document = value();
    property(handles[EMLITE_GLOBALTHIS], "document", document);
    method(document, "querySelectorAll", return_input);
    selected_input = collection(nullptr);

    double width = ui_max_width(".lyric-line");

    EXPECT_DOUBLE_EQ(width, 0);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

static Value* save_continuations(Value*, Value* const* arguments, unsigned)
{
    resolve_file = arguments[0];
    reject_file = arguments[1];
    return value();
}

static Value* file_promise(Value*, Value* const*, unsigned)
{
    Value* promise = value();
    method(promise, "then", save_continuations);
    return promise;
}

static void prepare_file(double size)
{
    Value *document = value(), *file = value();
    selected_input = value();
    property(file, "size", number(size));
    method(file, "arrayBuffer", file_promise);
    property(selected_input, "files", collection(file));
    property(selected_input, "value", string("selected.yaml"));
    method(document, "querySelector", return_input);
    property(handles[EMLITE_GLOBALTHIS], "document", document);
}

TEST_F(Given_BrowserBindings, When_FileExceedsIntegerRange_Then_SizeCannotWrap)
{
    prepare_file(4294967300.0);

    int size = ui_file_size("#file");
    ui_file_read("#file", 1024);

    EXPECT_EQ(size, INT_MAX);
    EXPECT_EQ(file_errors, 1);
    EXPECT_EQ(resolve_file, nullptr);
    EXPECT_EQ(live_handles, 0);
}

TEST_F(Given_BrowserBindings, When_FilePromiseResolves_Then_CImportsBytesAndReleasesInput)
{
    prepare_file(5);
    Value* bytes = string("hello");
    property(bytes, "byteLength", number(5));
    ui_file_read("#file", 32);
    ASSERT_NE(resolve_file, nullptr);

    invoke(resolve_file, bytes);

    EXPECT_EQ(imported_count, 1);
    EXPECT_STREQ(imported_text, "hello");
    EXPECT_STREQ(get(selected_input, "value")->text, "");
    EXPECT_EQ(file_errors, 0);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_FilePromiseRejects_Then_CReportsFailureAndReleasesInput)
{
    prepare_file(5);
    ui_file_read("#file", 32);
    ASSERT_NE(reject_file, nullptr);

    invoke(reject_file, string("read failed"));

    EXPECT_EQ(imported_count, 0);
    EXPECT_EQ(file_errors, 1);
    EXPECT_STREQ(get(selected_input, "value")->text, "");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_ResolvedFileExceedsLimit_Then_CRejectsBeforeCopying)
{
    prepare_file(5);
    Value* bytes = string("actual file contents exceed limit");
    property(bytes, "byteLength", number(32));
    ui_file_read("#file", 8);
    ASSERT_NE(resolve_file, nullptr);

    invoke(resolve_file, bytes);

    EXPECT_EQ(imported_count, 0);
    EXPECT_EQ(file_errors, 1);
    EXPECT_EQ(live_handles, 0);
}

static Value* save_listener(Value*, Value* const* arguments, unsigned)
{
    listener = arguments[1];
    return nullptr;
}

static char keys_seen[3][16];

static void nested_event(int)
{
    ui_event_key(keys_seen[0], sizeof(keys_seen[0]));
    Value* inner = value();
    property(inner, "key", string("inner"));
    on_event = nullptr;
    invoke(listener, inner);
    ui_event_key(keys_seen[1], sizeof(keys_seen[1]));
}

TEST_F(Given_BrowserBindings, When_EventDispatchReenters_Then_OuterEventIsRestored)
{
    method(handles[EMLITE_GLOBALTHIS], "addEventListener", save_listener);
    ui_listen("window", "keydown", APP_EVENT_KEY);
    Value* outer = value();
    property(outer, "key", string("outer"));
    on_event = nested_event;
    ASSERT_NE(listener, nullptr);

    invoke(listener, outer);
    ui_event_key(keys_seen[2], sizeof(keys_seen[2]));
    ui_shutdown();

    EXPECT_EQ(event_count, 2);
    EXPECT_STREQ(keys_seen[0], "outer");
    EXPECT_STREQ(keys_seen[1], "outer");
    EXPECT_STREQ(keys_seen[2], "");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_PathAndHashFit_Then_BothAreCopied)
{
    Value* location = value();
    property(location, "pathname", string("/"));
    property(location, "hash", string("#draft"));
    property(handles[EMLITE_GLOBALTHIS], "location", location);
    char path[8] = {};

    int copied = ui_path(path, sizeof(path));

    EXPECT_EQ(copied, 7);
    EXPECT_STREQ(path, "/#draft");
    EXPECT_EQ(live_handles, 0);
}

TEST_F(Given_BrowserBindings, When_PathAndHashExceedBuffer_Then_NoPartialRouteIsReturned)
{
    Value* location = value();
    property(location, "pathname", string("/"));
    property(location, "hash", string("#draft"));
    property(handles[EMLITE_GLOBALTHIS], "location", location);
    char path[7] = {};

    int copied = ui_path(path, sizeof(path));

    EXPECT_EQ(copied, -1);
    EXPECT_STREQ(path, "");
    EXPECT_EQ(live_handles, 0);
}

static Value* artwork_image;
static int created_images, artwork_results;
static unsigned artwork_ticket;

static Value* save_artwork_listener(Value* image, Value* const* arguments, unsigned)
{
    property(image, arguments[0]->text, arguments[1]);
    return nullptr;
}

static Value* create_artwork_image(Value*, Value* const* arguments, unsigned)
{
    created_images += !strcmp(arguments[0]->text, "img");
    artwork_image = value();
    method(artwork_image, "addEventListener", save_artwork_listener);
    return artwork_image;
}

static void artwork_result(void*, unsigned ticket, const char*)
{
    ++artwork_results;
    artwork_ticket = ticket;
}

static void prepare_artwork(void)
{
    artwork_image = nullptr;
    created_images = artwork_results = 0;
    artwork_ticket = 0;
    Value* document = value();
    method(document, "createElement", create_artwork_image);
    property(handles[EMLITE_GLOBALTHIS], "document", document);
    browser_set_callbacks(BrowserCallbacks{.context = nullptr,
                                           .event = fake_event,
                                           .file = fake_file,
                                           .file_error = fake_file_error,
                                           .artwork = artwork_result});
}

TEST_F(Given_BrowserBindings, When_ArtworkRequested_Then_DocumentCreatesImageWithCors)
{
    prepare_artwork();

    ui_artwork_load("https://example.test/cover.jpg", 41);
    Value* image = artwork_image;
    bool created = image != nullptr;
    const char* source = image ? get(image, "src")->text : "";
    bool url = !strcmp(source, "https://example.test/cover.jpg");
    const char* cors = image ? get(image, "crossOrigin")->text : "";
    bool anonymous = !strcmp(cors, "anonymous");
    ui_artwork_load("", 42);

    EXPECT_TRUE(created);
    EXPECT_EQ(created_images, 1);
    EXPECT_TRUE(url);
    EXPECT_TRUE(anonymous);
    EXPECT_EQ(artwork_results, 0);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_ArtworkFails_Then_CallbackAndImageOwnershipAreReleased)
{
    prepare_artwork();
    ui_artwork_load("https://example.test/cover.jpg", 41);
    ASSERT_NE(artwork_image, nullptr);
    Value* failed = get(artwork_image, "error");
    ASSERT_NE(failed, nullptr);

    invoke(failed, value());

    EXPECT_EQ(artwork_results, 1);
    EXPECT_EQ(artwork_ticket, 41u);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

typedef struct
{
    Value* callback;
    double delay;
    int cancelled;
} FakeTimer;

static FakeTimer timers[16];
static int timer_count;
static Value *toast, *toast_list;

static Value* start_timer(Value*, Value* const* arguments, unsigned)
{
    timers[timer_count] = {arguments[0], arguments[1]->number, 0};
    return number(++timer_count);
}

static Value* cancel_timer(Value*, Value* const* arguments, unsigned)
{
    timers[(int)arguments[0]->number - 1].cancelled = 1;
    return nullptr;
}

static Value* toast_element(Value*, Value* const*, unsigned)
{
    return toast;
}

static Value* toast_elements(Value*, Value* const*, unsigned)
{
    return toast_list;
}

static Value* set_toast_property(Value* receiver, Value* const* arguments, unsigned)
{
    property(receiver, arguments[0]->text, arguments[1]);
    return nullptr;
}

static void prepare_toast(void)
{
    timer_count = 0;
    memset(timers, 0, sizeof(timers));
    toast = value();
    Value* style = value();
    method(style, "setProperty", set_toast_property);
    property(toast, "style", style);
    method(toast, "setAttribute", set_toast_property);
    toast_list = collection(toast);
    method(toast_list, "item", toast_element);
    Value* document = value();
    method(document, "querySelector", toast_element);
    method(document, "querySelectorAll", toast_elements);
    property(handles[EMLITE_GLOBALTHIS], "document", document);
    method(handles[EMLITE_GLOBALTHIS], "setTimeout", start_timer);
    method(handles[EMLITE_GLOBALTHIS], "clearTimeout", cancel_timer);
}

TEST_F(Given_BrowserBindings, When_ToastExpires_Then_ItFadesBeforeClearingAndReleasesHandles)
{
    prepare_toast();
    ui_toast("Saved.");
    ASSERT_EQ(timer_count, 1);

    invoke(timers[0].callback, value());
    bool fading = !strcmp(get(toast, "data-toast-state")->text, "fading");
    bool transparent = !strcmp(get(get(toast, "style"), "opacity")->text, "0");
    bool text_remains = !strcmp(get(toast, "textContent")->text, "Saved.");
    invoke(timers[1].callback, value());

    EXPECT_TRUE(fading);
    EXPECT_TRUE(transparent);
    EXPECT_TRUE(text_remains);
    EXPECT_EQ(timer_count, 2);
    EXPECT_EQ(timers[0].delay, 3500);
    EXPECT_EQ(timers[1].delay, 200);
    EXPECT_STREQ(get(toast, "textContent")->text, "");
    EXPECT_STREQ(get(get(toast, "style"), "display")->text, "none");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_ToastIsReplaced_Then_PreviousTimerIsCancelledAndNewOneStarts)
{
    prepare_toast();
    ui_toast("First");
    ASSERT_EQ(timer_count, 1);

    ui_toast("Second");
    bool first_cancelled = timers[0].cancelled;
    bool second_pending = !timers[1].cancelled;
    bool second_visible = !strcmp(get(toast, "textContent")->text, "Second");
    invoke(timers[1].callback, value());
    invoke(timers[2].callback, value());

    EXPECT_TRUE(first_cancelled);
    EXPECT_TRUE(second_pending);
    EXPECT_TRUE(second_visible);
    EXPECT_EQ(timers[1].delay, 3500);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_FileSelectionErrorReplacesFadingToast_Then_ItAlsoExpires)
{
    prepare_toast();
    ui_toast("First");
    ASSERT_EQ(timer_count, 1);
    invoke(timers[0].callback, value());
    ASSERT_EQ(timer_count, 2);

    ui_toast("Choose a .yaml or .yml file.");
    bool previous_cancelled = timers[1].cancelled;
    bool visible = !strcmp(get(toast, "textContent")->text, "Choose a .yaml or .yml file.") &&
                   !strcmp(get(toast, "data-toast-state")->text, "visible") &&
                   !strcmp(get(get(toast, "style"), "opacity")->text, "1");
    invoke(timers[2].callback, value());
    invoke(timers[3].callback, value());

    EXPECT_TRUE(previous_cancelled);
    EXPECT_TRUE(visible);
    EXPECT_EQ(timer_count, 4);
    EXPECT_EQ(timers[2].delay, 3500);
    EXPECT_STREQ(get(toast, "textContent")->text, "");
    EXPECT_STREQ(get(get(toast, "style"), "display")->text, "none");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_ViewShutsDownWithToastPending_Then_TimerAndCallbackAreReleased)
{
    prepare_toast();
    ui_toast("Saved.");
    ASSERT_EQ(timer_count, 1);

    ui_shutdown();

    EXPECT_TRUE(timers[0].cancelled);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_ToastCleared_Then_PendingTimerIsCancelled)
{
    prepare_toast();
    ui_toast("Saved.");
    ASSERT_EQ(timer_count, 1);

    ui_toast("");

    EXPECT_TRUE(timers[0].cancelled);
    EXPECT_EQ(timer_count, 1);
    EXPECT_STREQ(get(toast, "textContent")->text, "");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

static Value *discussion_nodes[8], *discussion_frame, *discussion_message;
static int discussion_created, discussion_messages, discussion_removed_listeners;
static char discussion_target[80];

static Value* discussion_attribute(Value* receiver, Value* const* args, unsigned)
{
    property(receiver, args[0]->text, args[1]);
    return nullptr;
}

static Value* discussion_append(Value* receiver, Value* const* args, unsigned)
{
    property(args[0], "parent", receiver);
    return args[0];
}

static Value* discussion_listen(Value* receiver, Value* const* args, unsigned)
{
    property(receiver, args[0]->text, args[1]);
    return nullptr;
}

static Value* discussion_unlisten(Value*, Value* const*, unsigned)
{
    ++discussion_removed_listeners;
    return nullptr;
}

static Value* discussion_remove(Value*, Value* const*, unsigned)
{
    return nullptr;
}

static Value* discussion_query(Value*, Value* const*, unsigned)
{
    return discussion_frame;
}

static Value* discussion_post(Value*, Value* const* args, unsigned)
{
    ++discussion_messages;
    discussion_message = args[0];
    snprintf(discussion_target, sizeof(discussion_target), "%s", args[1]->text);
    return nullptr;
}

static Value* discussion_element(Value*, Value* const* args, unsigned)
{
    Value* element = value();
    property(element, "tag", args[0]);
    method(element, "setAttribute", discussion_attribute);
    method(element, "appendChild", discussion_append);
    method(element, "addEventListener", discussion_listen);
    method(element, "removeEventListener", discussion_unlisten);
    method(element, "querySelector", discussion_query);
    method(element, "remove", discussion_remove);
    discussion_nodes[discussion_created++] = element;
    return element;
}

static void prepare_discussion(void)
{
    memset(discussion_nodes, 0, sizeof(discussion_nodes));
    discussion_created = discussion_messages = discussion_removed_listeners = 0;
    discussion_target[0] = 0;
    discussion_message = nullptr;
    discussion_frame = value();
    method(discussion_frame, "addEventListener", discussion_listen);
    method(discussion_frame, "removeEventListener", discussion_unlisten);
    Value* window = value();
    method(window, "postMessage", discussion_post);
    property(discussion_frame, "contentWindow", window);
    Value *document = value(), *body = value();
    method(body, "appendChild", discussion_append);
    property(document, "body", body);
    method(document, "createElement", discussion_element);
    property(handles[EMLITE_GLOBALTHIS], "document", document);
    method(handles[EMLITE_GLOBALTHIS], "addEventListener", discussion_listen);
    method(handles[EMLITE_GLOBALTHIS], "removeEventListener", discussion_unlisten);
}

TEST_F(Given_BrowserBindings, When_DiscussionHiddenBeforeFirstSong_Then_NoExternalScriptIsLoaded)
{
    prepare_discussion();

    ui_discussion(nullptr, nullptr, nullptr);

    EXPECT_EQ(discussion_created, 0);
    EXPECT_EQ(live_handles, 0);
}

TEST_F(Given_BrowserBindings, When_DiscussionShown_Then_OfficialScriptHasPublicConfiguration)
{
    prepare_discussion();

    ui_discussion("song:avalon", "A song", "https://example.test/s/avalon/");
    Value* script = discussion_nodes[2];
    const char* source = get(script, "src")->text;
    const char* repo = get(script, "data-repo")->text;
    const char* repo_id = get(script, "data-repo-id")->text;
    const char* category_id = get(script, "data-category-id")->text;
    bool stable_term = !strcmp(get(script, "data-term")->text, "song:avalon");
    bool specific = !strcmp(get(script, "data-mapping")->text, "specific");
    bool reactions = !strcmp(get(script, "data-reactions-enabled")->text, "1");
    bool lazy = !strcmp(get(script, "data-loading")->text, "lazy");
    bool external_only = get(script, "textContent") == nullptr;
    ui_shutdown();

    EXPECT_EQ(discussion_created, 3);
    EXPECT_STREQ(source, "https://giscus.app/client.js");
    EXPECT_STREQ(repo, ONLYC_STRING_GISCUS_REPO);
    EXPECT_STREQ(repo_id, ONLYC_STRING_GISCUS_REPO_ID);
    EXPECT_STREQ(category_id, ONLYC_STRING_GISCUS_CATEGORY_ID);
    EXPECT_TRUE(stable_term);
    EXPECT_TRUE(specific);
    EXPECT_TRUE(reactions);
    EXPECT_TRUE(lazy);
    EXPECT_TRUE(external_only);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_SongChangesDuringScriptDownload_Then_LatestTermReachesIframe)
{
    prepare_discussion();
    ui_discussion("song:avalon", "First", "https://example.test/s/avalon/");
    ASSERT_EQ(discussion_created, 3);

    ui_discussion("song:faded", "Second", "https://example.test/s/faded/");
    bool latest_attribute = !strcmp(get(discussion_nodes[2], "data-term")->text, "song:faded");
    invoke(get(discussion_nodes[2], "load"), value());
    Value* config = get(get(discussion_message, "giscus"), "setConfig");
    const char* term = get(config, "term")->text;
    const char* description = get(config, "description")->text;
    const char* backlink = get(config, "backLink")->text;
    ui_shutdown();

    EXPECT_TRUE(latest_attribute);
    EXPECT_EQ(discussion_created, 3);
    EXPECT_EQ(discussion_messages, 1);
    EXPECT_STREQ(term, "song:faded");
    EXPECT_STREQ(description, "Second");
    EXPECT_STREQ(backlink, "https://example.test/s/faded/");
    EXPECT_STREQ(discussion_target, "https://giscus.app");
    EXPECT_EQ(discussion_removed_listeners, 3);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_LoadedWidgetNavigates_Then_IframeIsReusedAndConfigurationUpdates)
{
    prepare_discussion();
    ui_discussion("song:avalon", "First", "https://example.test/s/avalon/");
    ASSERT_EQ(discussion_created, 3);
    invoke(get(discussion_nodes[2], "load"), value());

    ui_discussion(nullptr, nullptr, nullptr);
    bool hidden = get(discussion_nodes[0], "hidden")->number == 1;
    ui_discussion("song:faded", "Second", "https://example.test/s/faded/");
    bool shown = get(discussion_nodes[0], "hidden")->number == 0;
    invoke(get(discussion_frame, "load"), value());
    Value* config = get(get(discussion_message, "giscus"), "setConfig");
    const char* term = get(config, "term")->text;
    ui_shutdown();

    EXPECT_TRUE(hidden);
    EXPECT_TRUE(shown);
    EXPECT_EQ(discussion_created, 3);
    EXPECT_EQ(discussion_messages, 3);
    EXPECT_STREQ(term, "song:faded");
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_ShutdownPrecedesScriptLoad_Then_PendingCallbackIsReleased)
{
    prepare_discussion();
    ui_discussion("song:avalon", "", "https://example.test/s/avalon/");
    ASSERT_EQ(discussion_created, 3);

    ui_shutdown();

    EXPECT_EQ(discussion_removed_listeners, 2);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

static Value* discussion_resize_event(const char* origin, Value* source)
{
    Value *event = value(), *data = value(), *payload = value();
    property(event, "origin", string(origin));
    property(event, "source", source);
    property(payload, "resizeHeight", number(500));
    property(data, "giscus", payload);
    property(event, "data", data);
    return event;
}

TEST_F(Given_BrowserBindings, When_WidgetHydratesAfterLoad_Then_LatestConfigurationIsRetriedOnce)
{
    prepare_discussion();
    ui_discussion("song:avalon", "First", "https://example.test/s/avalon/");
    ASSERT_EQ(discussion_created, 3);
    invoke(get(discussion_nodes[2], "load"), value());
    Value* event =
        discussion_resize_event("https://giscus.app", get(discussion_frame, "contentWindow"));
    int before = discussion_messages;

    invoke(get(handles[EMLITE_GLOBALTHIS], "message"), event);
    int after_first = discussion_messages;
    invoke(get(handles[EMLITE_GLOBALTHIS], "message"), event);
    ui_shutdown();

    EXPECT_EQ(after_first, before + 1);
    EXPECT_EQ(discussion_messages, after_first);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}

TEST_F(Given_BrowserBindings, When_UnrelatedFrameSendsResize_Then_ConfigurationIsNotSent)
{
    prepare_discussion();
    ui_discussion("song:avalon", "First", "https://example.test/s/avalon/");
    ASSERT_EQ(discussion_created, 3);
    invoke(get(discussion_nodes[2], "load"), value());
    Value* wrong_origin =
        discussion_resize_event("https://example.test", get(discussion_frame, "contentWindow"));
    Value* wrong_source = discussion_resize_event("https://giscus.app", value());
    int before = discussion_messages;

    invoke(get(handles[EMLITE_GLOBALTHIS], "message"), wrong_origin);
    invoke(get(handles[EMLITE_GLOBALTHIS], "message"), wrong_source);
    ui_shutdown();

    EXPECT_EQ(discussion_messages, before);
    EXPECT_EQ(live_handles, 0);
    EXPECT_EQ(invalid_releases, 0);
}
