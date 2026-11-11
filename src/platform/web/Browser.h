#ifndef ONLYC_PLATFORM_WEB_BROWSER_H
#define ONLYC_PLATFORM_WEB_BROWSER_H

#include <stddef.h>

/* Browser operations implemented in C using generated Web IDL bindings. */
#ifdef __wasm__
#define EXPORT(name) __attribute__((export_name(name)))
#else
#define EXPORT(name)
#endif
typedef struct
{
    void* context;
    void (*event)(void* context, int code);
    void (*file)(void* context, const char* text, size_t length);
    void (*file_error)(void* context);
    void (*artwork)(void* context, unsigned request, const char* color);
} BrowserCallbacks;

void browser_set_callbacks(BrowserCallbacks callbacks);
void ui_shutdown(void);
int ui_read(const char* id, char* text, int capacity);
int ui_path(char* text, int capacity);
void ui_title(const char* text);
void ui_route(const char* path);
void ui_focus(const char* id);
void ui_reveal(const char* id);
void ui_set_text(const char* selector, const char* text);
void ui_toast(const char* text);
/* A null term hides the persistent third-party widget. */
void ui_discussion(const char* term, const char* description, const char* backlink);
int ui_confirm(const char* message);
void ui_style(const char* selector, const char* property, const char* value);
void ui_clear_styles(const char* selector);
void ui_attribute(const char* selector, const char* attribute, const char* value);
int ui_width(void);
/* Maximum rendered border-box width, in CSS pixels; zero for no matches. */
double ui_max_width(const char* selector);
int ui_count(const char* selector);
void ui_listen(const char* target, const char* event, int code);
int ui_event_text(const char* selector, const char* property, char* value, int capacity);
int ui_event_key(char* value, int capacity);
int ui_event_shift(void);
int ui_event_plain_click(void);
int ui_event_has_files(void);
int ui_drop_count(void);
int ui_drop_name(char* name, int capacity);
int ui_drop_size(void);
void ui_drop_read(int capacity);
void ui_prevent_default(void);
int ui_active_matches(const char* selector);
int ui_active_index(const char* selector);
void ui_focus_at(const char* selector, int index);
int ui_file_size(const char* selector);
void ui_file_read(const char* selector, int capacity);
void ui_click(const char* selector);
int ui_blob_url(const char* text, unsigned length, const char* mime, char* url, int capacity);
void ui_revoke_url(const char* url, int delay_ms);
void ui_artwork_load(const char* url, unsigned request);
typedef int Node;
Node dom_reset(const char* selector);
Node dom_find(const char* selector);
Node dom_create(Node parent, const char* tag);
void dom_text(Node parent, const char* text, int length);
void dom_attr(Node node, const char* name, const char* value);
void dom_value(Node node, const char* value);
void dom_clear(Node node);
void dom_remove(Node node);
#endif
