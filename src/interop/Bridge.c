#include "interop/Bridge.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static DomValue* temporaries;
static size_t temporary_count;
static size_t temporary_capacity;
static BridgeCallback** callbacks;
static size_t callback_capacity;

static DomValue scoped(DomValue value)
{
    /* Emlite's reserved values are immortal. Its public decrement operation is
     * a no-op for them; only nonreserved handles require ownership tracking. */
    if (value.h <= EMLITE_RESERVED)
    {
        return value;
    }
    if (temporary_count == temporary_capacity)
    {
        size_t capacity = temporary_capacity ? temporary_capacity * 2 : 64;
        DomValue* grown = realloc(temporaries, capacity * sizeof(*grown));
        if (!grown)
        {
            abort();
        }
        temporaries = grown;
        temporary_capacity = capacity;
    }
    temporaries[temporary_count++] = value;
    return value;
}

BridgeScope bridge_scope_begin(void)
{
    return temporary_count;
}

void bridge_scope_end(BridgeScope scope)
{
    while (temporary_count > scope)
    {
        em_Val_delete(temporaries[--temporary_count]);
    }
}

DomValue bridge_retain(DomValue value)
{
    if (value.h > EMLITE_RESERVED)
    {
        emlite_val_inc_ref(value.h);
    }
    return value;
}

void bridge_release(DomValue value)
{
    em_Val_delete(value);
}

DomValue bridge_string(const char* text)
{
    return bridge_string_n(text, strlen(text));
}

DomValue bridge_string_n(const char* text, size_t length)
{
    return scoped(em_Val_from_handle(emlite_val_make_str(text, length)));
}

bool bridge_present(DomValue value)
{
    return value.h != EMLITE_NULL && value.h != EMLITE_UNDEFINED;
}

int bridge_copy_string(DomValue value, char* text, int capacity)
{
    if (capacity <= 0)
    {
        return -1;
    }
    text[0] = '\0';
    if (!bridge_present(value))
    {
        return 0;
    }
    char* copy = em_Val_as_string(value);
    size_t length = copy ? strlen(copy) : 0;
    int result = -1;
    if (length < (size_t)capacity)
    {
        if (copy)
        {
            memcpy(text, copy, length + 1);
        }
        result = (int)length;
    }
    free(copy);
    return result;
}

int bridge_copy_bytes(DomValue buffer, void* destination, unsigned capacity)
{
    BridgeScope scope = bridge_scope_begin();
    double length = em_Val_as_double(bridge_get(buffer, "byteLength"));
    int result = -1;
    if (length >= 0 && length <= capacity && length <= INT_MAX)
    {
        DomValue bytes = bridge_construct("Uint8Array", &buffer, 1, NULL, 0);
        DomValue arguments[] = {bytes, bridge_number((double)(uintptr_t)destination)};
        /* Emlite refreshes its public heap views when the property-name string
         * crosses the boundary. Fetch the heap last, after C allocations that
         * might grow WASM memory. Uint8Array.set performs the bounded copy. */
        DomValue heap = bridge_global("HEAPU8");
        DomValue copied = bridge_call(heap, "set", arguments, 2, NULL, 0);
        if (!bridge_is_error(copied))
        {
            result = (int)length;
        }
    }
    bridge_scope_end(scope);
    return result;
}

DomValue bridge_number(double number)
{
    return scoped(em_Val_from_double(number));
}

DomValue bridge_boolean(bool boolean)
{
    return scoped(em_Val_from_bool(boolean));
}

DomValue bridge_object(void)
{
    return scoped(em_Val_object());
}

DomValue bridge_get(DomValue object, const char* name)
{
    if (!bridge_present(object))
    {
        return em_Val_undefined();
    }
    DomValue key = em_Val_from_string(name);
    DomValue value = em_Val_get(object, key);
    em_Val_delete(key);
    return scoped(value);
}

void bridge_set(DomValue object, const char* name, DomValue value)
{
    DomValue key = em_Val_from_string(name);
    em_Val_set(object, key, value);
    em_Val_delete(key);
}

DomValue bridge_global(const char* name)
{
    /* em_Val_global allocates a property-name handle without releasing it in
     * the pinned version. Use its public get API with explicit key ownership. */
    return bridge_get(em_Val_global_this(), name);
}

DomValue bridge_at(DomValue object, unsigned index)
{
    if (!bridge_present(object))
    {
        return em_Val_undefined();
    }
    DomValue key = em_Val_from_uint(index);
    DomValue value = em_Val_get(object, key);
    em_Val_delete(key);
    return scoped(value);
}

static Handle
argument_array(const DomValue* arguments, size_t count, const DomValue* rest, size_t rest_count)
{
    Handle array = emlite_val_new_array();
    /* The low-level call ABI wants numeric handles here, not a JS value array. */
    for (size_t i = 0; i < count; ++i)
    {
        emlite_val_push(array, arguments[i].h);
    }
    for (size_t i = 0; i < rest_count; ++i)
    {
        emlite_val_push(array, rest[i].h);
    }
    return array;
}

DomValue bridge_call(DomValue object,
                     const char* method,
                     const DomValue* arguments,
                     size_t count,
                     const DomValue* rest,
                     size_t rest_count)
{
    Handle array = argument_array(arguments, count, rest, rest_count);
    Handle result = emlite_val_obj_call(object.h, method, strlen(method), array);
    emlite_val_dec_ref(array);
    return scoped(em_Val_from_handle(result));
}

DomValue bridge_construct(const char* name,
                          const DomValue* arguments,
                          size_t count,
                          const DomValue* rest,
                          size_t rest_count)
{
    DomValue constructor = bridge_global(name);
    Handle array = argument_array(arguments, count, rest, rest_count);
    Handle result = emlite_val_construct_new(constructor.h, array);
    emlite_val_dec_ref(array);
    return scoped(em_Val_from_handle(result));
}

bool bridge_is_error(DomValue value)
{
    return em_Val_instanceof(value, bridge_global("Error"));
}

bool bridge_string_equals(DomValue value, const char* expected)
{
    if (!em_Val_is_string(value))
    {
        return false;
    }
    /* The pinned runtime returns a null pointer for an empty string. Compare
     * that value directly so it stays distinct from JS null and undefined. */
    if (!*expected)
    {
        return em_Val_seq(value, bridge_string(""));
    }
    char* text = em_Val_as_string(value);
    bool equal = text && strcmp(text, expected) == 0;
    free(text);
    return equal;
}

static Handle dispatch(Handle arguments, Handle context)
{
    BridgeCallback* callback = context < callback_capacity ? callbacks[context] : NULL;
    BridgeScope scope = bridge_scope_begin();
    if (callback && callback->handler)
    {
        callback->handler(em_Val_from_handle(arguments), callback->context);
    }
    bridge_scope_end(scope);
    /* Emlite transfers the freshly allocated argument-array handle to C.
     * Release it once per invocation, including when the callback is inactive. */
    emlite_val_dec_ref(arguments);
    return EMLITE_UNDEFINED;
}

void bridge_callback_init(BridgeCallback* callback, BridgeHandler handler, void* context)
{
    size_t slot = 1;
    while (slot < callback_capacity && callbacks[slot])
    {
        ++slot;
    }
    if (slot >= callback_capacity)
    {
        size_t capacity = callback_capacity ? callback_capacity * 2 : 32;
        BridgeCallback** grown = realloc(callbacks, capacity * sizeof(*grown));
        if (!grown)
        {
            abort();
        }
        memset(grown + callback_capacity, 0, (capacity - callback_capacity) * sizeof(*grown));
        callbacks = grown;
        callback_capacity = capacity;
    }
    callbacks[slot] = callback;
    callback->token = (Handle)slot;
    callback->handler = handler;
    callback->context = context;
    callback->function = em_Val_make_fn(dispatch, callback->token);
}

void bridge_callback_destroy(BridgeCallback* callback)
{
    if (callback->token)
    {
        callbacks[callback->token] = NULL;
        callback->token = 0;
    }
    em_Val_delete(callback->function);
    callback->handler = NULL;
    callback->function = em_Val_undefined();
}
