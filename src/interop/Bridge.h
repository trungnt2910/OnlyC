#ifndef ONLYC_INTEROP_BRIDGE_H
#define ONLYC_INTEROP_BRIDGE_H

#include <stdio.h>

/* This include must follow stdio: emcore's freestanding redeclarations must
 * inherit libc's C++ exception specifications in native GoogleTest builds. */
#include <emlite/emlite.h>

/* Generated types are intentionally aliases. All returned values belong to the
 * current scope. Inputs are borrowed. Retain/release only for longer lifetimes. */
typedef em_Val DomValue;
typedef size_t BridgeScope;
typedef void (*BridgeHandler)(DomValue arguments, void* context);

/* The caller owns this record and must keep it alive while JS can invoke the
 * function. Remove the listener before destroying the callback. These
 * callbacks are event handlers: their JavaScript return value is ignored. */
typedef struct
{
    BridgeHandler handler;
    void* context;
    DomValue function;
    Handle token;
} BridgeCallback;

BridgeScope bridge_scope_begin(void);
void bridge_scope_end(BridgeScope scope);
DomValue bridge_retain(DomValue value);
void bridge_release(DomValue value);
DomValue bridge_string(const char* text);
DomValue bridge_string_n(const char* text, size_t length);
int bridge_copy_string(DomValue value, char* text, int capacity);
int bridge_copy_bytes(DomValue buffer, void* destination, unsigned capacity);
bool bridge_present(DomValue value);
DomValue bridge_number(double number);
DomValue bridge_boolean(bool boolean);
DomValue bridge_object(void);
DomValue bridge_global(const char* name);
DomValue bridge_get(DomValue object, const char* name);
void bridge_set(DomValue object, const char* name, DomValue value);
DomValue bridge_at(DomValue object, unsigned index);
DomValue bridge_call(DomValue object,
                     const char* method,
                     const DomValue* arguments,
                     size_t count,
                     const DomValue* rest,
                     size_t rest_count);
DomValue bridge_construct(const char* name,
                          const DomValue* arguments,
                          size_t count,
                          const DomValue* rest,
                          size_t rest_count);
bool bridge_is_error(DomValue value);
bool bridge_string_equals(DomValue value, const char* expected);
void bridge_callback_init(BridgeCallback* callback, BridgeHandler handler, void* context);
void bridge_callback_destroy(BridgeCallback* callback);

#endif
