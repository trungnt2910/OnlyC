#include "core/Song.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void buffer_append(Buffer* buffer, const char* text, size_t length)
{
    if (buffer->failed)
    {
        return;
    }
    if (length > 8u * MAX_IMPORT || buffer->len > 8u * MAX_IMPORT - length)
    {
        buffer->failed = 1;
        return;
    }
    if (buffer->len + length + 1 > buffer->capacity)
    {
        size_t capacity = (buffer->len + length + 1) * 2;
        char* resized = realloc(buffer->data, capacity);
        if (!resized)
        {
            buffer->failed = 1;
            return;
        }
        buffer->data = resized;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->len, text, length);
    buffer->len += length;
    buffer->data[buffer->len] = 0;
}

void buffer_puts(Buffer* buffer, const char* text)
{
    buffer_append(buffer, text, strlen(text));
}

void buffer_html(Buffer* buffer, const char* text, size_t length)
{
    for (size_t i = 0; i < length; ++i)
    {
        switch (text[i])
        {
        case '&':
            buffer_puts(buffer, "&amp;");
            break;
        case '<':
            buffer_puts(buffer, "&lt;");
            break;
        case '>':
            buffer_puts(buffer, "&gt;");
            break;
        case '"':
            buffer_puts(buffer, "&quot;");
            break;
        case '\'':
            buffer_puts(buffer, "&#39;");
            break;
        default:
            buffer_append(buffer, text + i, 1);
            break;
        }
    }
}

void buffer_free(Buffer* buffer)
{
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}
