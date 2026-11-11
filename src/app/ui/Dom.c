#include "app/ui/Dom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <entity.h>
#include <md4c.h>

#include "core/Song.h"

Node element(Node parent, const char* tag, const char* class_name, const char* text)
{
    Node node = dom_create(parent, tag);
    if (class_name)
    {
        dom_attr(node, "class", class_name);
    }
    if (text)
    {
        dom_text(node, text, (int)strlen(text));
    }
    return node;
}

void attribute_number(Node node, const char* name, int value)
{
    char number[32];
    snprintf(number, sizeof(number), "%d", value);
    dom_attr(node, name, number);
}

typedef struct
{
    Node stack[256];
    size_t depth;
} Markdown;

static void decode_text(Buffer* out, MD_TEXTTYPE type, const char* text, MD_SIZE size);

static int push(Markdown* md, const char* tag)
{
    if (md->depth + 1 == sizeof(md->stack) / sizeof(*md->stack))
    {
        return -1;
    }
    Node next = element(md->stack[md->depth], tag, NULL, NULL);
    md->stack[++md->depth] = next;
    return 0;
}

static const char* block_tag(MD_BLOCKTYPE type)
{
    switch (type)
    {
    case MD_BLOCK_QUOTE:
        return "blockquote";
    case MD_BLOCK_UL:
        return "ul";
    case MD_BLOCK_OL:
        return "ol";
    case MD_BLOCK_LI:
        return "li";
    case MD_BLOCK_H:
        return "h3";
    case MD_BLOCK_CODE:
        return "pre";
    case MD_BLOCK_P:
        return "p";
    default:
        return NULL;
    }
}

static int block_enter(MD_BLOCKTYPE type, void* detail, void* context)
{
    Markdown* md = context;
    const char* tag = block_tag(type);
    if (tag && push(md, tag))
    {
        return -1;
    }
    if (type == MD_BLOCK_OL)
    {
        attribute_number(md->stack[md->depth], "start", (int)((MD_BLOCK_OL_DETAIL*)detail)->start);
    }
    if (type == MD_BLOCK_HR)
    {
        element(md->stack[md->depth], "hr", NULL, NULL);
    }
    return 0;
}

static int block_leave(MD_BLOCKTYPE type, void* detail, void* context)
{
    (void)detail;
    Markdown* md = context;
    if (block_tag(type) && md->depth)
    {
        --md->depth;
    }
    return 0;
}

static const char* span_tag(MD_SPANTYPE type)
{
    switch (type)
    {
    case MD_SPAN_EM:
        return "em";
    case MD_SPAN_STRONG:
        return "strong";
    case MD_SPAN_CODE:
        return "code";
    case MD_SPAN_DEL:
        return "del";
    case MD_SPAN_A:
        return "a";
    default:
        return "span";
    }
}

static int span_enter(MD_SPANTYPE type, void* detail, void* context)
{
    Markdown* md = context;
    if (push(md, span_tag(type)))
    {
        return -1;
    }
    if (type == MD_SPAN_A)
    {
        MD_ATTRIBUTE* href = &((MD_SPAN_A_DETAIL*)detail)->href;
        Buffer url = {0};
        for (unsigned i = 0; href->substr_offsets[i] < href->size; ++i)
        {
            MD_OFFSET start = href->substr_offsets[i];
            decode_text(&url,
                        href->substr_types[i],
                        href->text + start,
                        href->substr_offsets[i + 1] - start);
        }
        if (!url.failed && url.data && safe_url(url.data))
        {
            Node node = md->stack[md->depth];
            dom_attr(node, "href", url.data);
            dom_attr(node, "target", "_blank");
            dom_attr(node, "rel", "noopener noreferrer");
        }
        buffer_free(&url);
    }
    return 0;
}

static int span_leave(MD_SPANTYPE type, void* detail, void* context)
{
    (void)type;
    (void)detail;
    Markdown* md = context;
    if (md->depth)
    {
        --md->depth;
    }
    return 0;
}

static void codepoint(Buffer* out, unsigned c)
{
    unsigned char text[4];
    int n;
    if (!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
    {
        c = 0xfffd;
    }
    if (c < 0x80)
    {
        text[0] = (unsigned char)c;
        n = 1;
    }
    else if (c < 0x800)
    {
        text[0] = 0xc0 | (c >> 6);
        text[1] = 0x80 | (c & 63);
        n = 2;
    }
    else if (c < 0x10000)
    {
        text[0] = 0xe0 | (c >> 12);
        text[1] = 0x80 | ((c >> 6) & 63);
        text[2] = 0x80 | (c & 63);
        n = 3;
    }
    else
    {
        text[0] = 0xf0 | (c >> 18);
        text[1] = 0x80 | ((c >> 12) & 63);
        text[2] = 0x80 | ((c >> 6) & 63);
        text[3] = 0x80 | (c & 63);
        n = 4;
    }
    buffer_append(out, (const char*)text, (size_t)n);
}

static void decode_text(Buffer* out, MD_TEXTTYPE type, const char* text, MD_SIZE size)
{
    if (type == MD_TEXT_NULLCHAR)
    {
        codepoint(out, 0xfffd);
    }
    else if (type == MD_TEXT_ENTITY)
    {
        const ENTITY* entity = entity_lookup(text, size);
        if (size > 3 && text[1] == '#')
        {
            size_t start = text[2] == 'x' || text[2] == 'X' ? 3 : 2;
            unsigned value = 0, base = start == 3 ? 16 : 10;
            /* MD4C has already validated the digits; stop before Unicode overflow. */
            for (size_t i = start; i + 1 < size && value <= 0x10ffff; ++i)
            {
                unsigned digit = (unsigned char)text[i];
                digit = digit <= '9' ? digit - '0' : (digit | 32u) - 'a' + 10;
                value = value * base + digit;
            }
            codepoint(out, value);
        }
        else if (entity)
        {
            codepoint(out, entity->codepoints[0]);
            if (entity->codepoints[1])
            {
                codepoint(out, entity->codepoints[1]);
            }
        }
        else
        {
            buffer_append(out, text, size);
        }
    }
    else
    {
        buffer_append(out, text, size);
    }
}

static int markdown_text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* context)
{
    Markdown* md = context;
    Node node = md->stack[md->depth];
    if (type == MD_TEXT_BR)
    {
        element(node, "br", NULL, NULL);
        return 0;
    }
    Buffer decoded = {0};
    decode_text(&decoded, type, text, size);
    int failed = decoded.failed;
    if (decoded.len)
    {
        dom_text(node, decoded.data, (int)decoded.len);
    }
    buffer_free(&decoded);
    return failed ? -1 : 0;
}

int dom_markdown(Node parent, const char* source)
{
    Markdown md = {{parent}, 0};
    MD_PARSER parser = {0,
                        MD_FLAG_NOHTML | MD_FLAG_STRIKETHROUGH,
                        block_enter,
                        block_leave,
                        span_enter,
                        span_leave,
                        markdown_text,
                        NULL,
                        NULL};
    return md_parse(source, (MD_SIZE)strlen(source), &parser, &md) == 0;
}
