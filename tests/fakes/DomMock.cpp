extern "C"
{
#include "fakes/DomMock.h"
}

#include <cstdio>
#include <cstring>

extern "C"
{
#include "app/ui/Dom.h"
}

/* A small DOM test double, not an HTML parser. The serializer is test-only. */
typedef struct
{
    int parent;
    char tag[24], id[80];
    Buffer text, attributes, value;
} FakeNode;

static FakeNode nodes[4096];
static int used;
static Buffer snapshot;
static int viewport_width = 1440;

typedef struct
{
    char selector[160], property[64], value[192];
} StyleCall;

static StyleCall styles[1024];
static int style_count;
int test_prevented, test_shift, test_active_index, test_file_bytes, test_read_requests;
char test_key[32], test_event_id[80];
int test_has_files, test_drop_count, test_drop_bytes, test_drop_reads;
int test_plain_click = 1;
char test_drop_filename[1024];
char test_discussion_term[96], test_discussion_description[8192], test_discussion_backlink[2048];

static int attached(Node node)
{
    while (node > 1)
    {
        node = nodes[node].parent;
    }
    return node == 1;
}

extern "C"
{
    void ui_discussion(const char* term, const char* description, const char* backlink)
    {
        snprintf(test_discussion_term, sizeof(test_discussion_term), "%s", term ? term : "");
        snprintf(test_discussion_description,
                 sizeof(test_discussion_description),
                 "%s",
                 description ? description : "");
        snprintf(test_discussion_backlink,
                 sizeof(test_discussion_backlink),
                 "%s",
                 backlink ? backlink : "");
    }

    void ui_reveal(const char*)
    {
    }

    void ui_artwork_load(const char*, unsigned)
    {
    }

    Node dom_reset(const char*)
    {
        for (int i = 1; i <= used; ++i)
        {
            buffer_free(&nodes[i].text);
            buffer_free(&nodes[i].attributes);
            buffer_free(&nodes[i].value);
        }
        memset(nodes, 0, sizeof(nodes));
        used = 1;
        strcpy(nodes[1].id, "app");
        return 1;
    }

    Node dom_find(const char* selector)
    {
        if (selector[0] != '#')
        {
            return 0;
        }
        for (int i = 1; i <= used; ++i)
        {
            if (attached(i) && !strcmp(nodes[i].id, selector + 1))
            {
                return i;
            }
        }
        return 0;
    }

    Node dom_create(Node parent, const char* tag)
    {
        if (used == 4095)
        {
            return 0;
        }
        Node node = ++used;
        nodes[node].parent = parent;
        snprintf(nodes[node].tag, sizeof(nodes[node].tag), "%s", tag);
        return node;
    }

    void dom_text(Node parent, const char* text, int len)
    {
        Node node = dom_create(parent, "#text");
        buffer_append(&nodes[node].text, text, (size_t)len);
    }

    void dom_attr(Node node, const char* name, const char* value)
    {
        if (!strcmp(name, "id"))
        {
            snprintf(nodes[node].id, sizeof(nodes[node].id), "%s", value);
        }
        buffer_puts(&nodes[node].attributes, " ");
        buffer_puts(&nodes[node].attributes, name);
        buffer_puts(&nodes[node].attributes, "=\"");
        buffer_html(&nodes[node].attributes, value, strlen(value));
        buffer_puts(&nodes[node].attributes, "\"");
    }

    void dom_value(Node node, const char* value)
    {
        buffer_free(&nodes[node].value);
        buffer_puts(&nodes[node].value, value);
    }

    void dom_clear(Node node)
    {
        for (int i = 1; i <= used; ++i)
        {
            if (nodes[i].parent == node)
            {
                nodes[i].parent = -1;
            }
        }
    }

    void dom_remove(Node node)
    {
        dom_clear(node);
        nodes[node].parent = -1;
    }

    void ui_style(const char* selector, const char* property, const char* value)
    {
        int i;
        for (i = 0; i < style_count; ++i)
        {
            if (!strcmp(styles[i].selector, selector) && !strcmp(styles[i].property, property))
            {
                break;
            }
        }
        if (i == 1024)
        {
            return;
        }
        if (i == style_count)
        {
            ++style_count;
        }
        snprintf(styles[i].selector, sizeof(styles[i].selector), "%s", selector);
        snprintf(styles[i].property, sizeof(styles[i].property), "%s", property);
        snprintf(styles[i].value, sizeof(styles[i].value), "%s", value);
    }

    void ui_clear_styles(const char*)
    {
        style_count = 0;
    }

    void ui_attribute(const char*, const char*, const char*)
    {
    }

    int ui_width(void)
    {
        return viewport_width;
    }

    double ui_max_width(const char*)
    {
        return 0;
    }

    int ui_count(const char* selector)
    {
        return !strcmp(selector, "#panel") ? !!dom_find(selector) : 3;
    }

    void ui_listen(const char*, const char*, int)
    {
    }

    int ui_event_text(const char*, const char*, char* value, int capacity)
    {
        return snprintf(value, (size_t)capacity, "%s", test_event_id);
    }

    int ui_event_key(char* value, int capacity)
    {
        return snprintf(value, (size_t)capacity, "%s", test_key);
    }

    int ui_event_plain_click(void)
    {
        return test_plain_click;
    }

    int ui_event_has_files(void)
    {
        return test_has_files;
    }

    int ui_drop_count(void)
    {
        return test_drop_count;
    }

    int ui_drop_size(void)
    {
        return test_drop_bytes;
    }

    int ui_drop_name(char* name, int capacity)
    {
        return snprintf(name, (size_t)capacity, "%s", test_drop_filename);
    }

    void ui_drop_read(int)
    {
        ++test_drop_reads;
    }

    int ui_event_shift(void)
    {
        return test_shift;
    }

    void ui_prevent_default(void)
    {
        ++test_prevented;
    }

    int ui_active_matches(const char*)
    {
        return 0;
    }

    int ui_active_index(const char*)
    {
        return test_active_index;
    }

    void ui_focus_at(const char*, int index)
    {
        test_active_index = index;
    }

    int ui_file_size(const char*)
    {
        return test_file_bytes;
    }

    void ui_file_read(const char*, int)
    {
        ++test_read_requests;
    }

    void ui_click(const char*)
    {
    }
}

static void serialize(Node node, Buffer* out)
{
    FakeNode* n = &nodes[node];
    if (!strcmp(n->tag, "#text"))
    {
        buffer_html(out, n->text.data, n->text.len);
        return;
    }
    if (n->tag[0])
    {
        buffer_puts(out, "<");
        buffer_puts(out, n->tag);
        if (n->attributes.data)
        {
            buffer_puts(out, n->attributes.data);
        }
        buffer_puts(out, ">");
    }
    for (int i = 1; i <= used; ++i)
    {
        if (nodes[i].parent == node)
        {
            serialize(i, out);
        }
    }
    if (n->tag[0])
    {
        buffer_puts(out, "</");
        buffer_puts(out, n->tag);
        buffer_puts(out, ">");
    }
}

const char* test_input_value(const char* id)
{
    char selector[96];
    snprintf(selector, sizeof(selector), "#%s", id);
    Node node = dom_find(selector);
    return node && nodes[node].value.data ? nodes[node].value.data : "";
}

const char* test_document(void)
{
    buffer_free(&snapshot);
    serialize(1, &snapshot);
    return snapshot.data ? snapshot.data : "";
}

const char* test_subtree(const char* selector)
{
    buffer_free(&snapshot);
    Node node = dom_find(selector);
    if (node)
    {
        serialize(node, &snapshot);
    }
    return snapshot.data ? snapshot.data : "";
}

int markdown_render(const char* source, Buffer* out)
{
    Node root = dom_reset("#app");
    int ok = dom_markdown(root, source);
    serialize(root, out);
    return ok;
}

void test_set_width(int value)
{
    viewport_width = value;
}

const char* test_style(const char* selector, const char* property)
{
    for (int i = 0; i < style_count; ++i)
    {
        if (!strcmp(styles[i].selector, selector) && !strcmp(styles[i].property, property))
        {
            return styles[i].value;
        }
    }
    return "";
}
