#include "app/view/impl/Controls.h"

#include <stdio.h>
#include <string.h>

Node view_button(
    Node parent, AppAction action, int index, const char* class_name, const char* label)
{
    Node node = element(parent, "button", class_name, label);
    dom_attr(node, "type", "button");
    attribute_number(node, "data-action", action);
    attribute_number(node, "data-index", index);
    return node;
}

Node view_field(
    Node parent, const char* id, const char* label, const char* value, int multiline, int max)
{
    Node wrapper = element(parent, "label", "field", NULL);
    dom_attr(wrapper, "for", id);
    element(wrapper, "span", NULL, label);
    Node input = element(wrapper, multiline ? "textarea" : "input", NULL, NULL);
    dom_attr(input, "id", id);
    attribute_number(input, "maxlength", max);
    if (multiline)
    {
        attribute_number(input, "rows", 5);
    }
    else
    {
        dom_attr(input, "type", "text");
    }
    dom_value(input, value);
    return input;
}

int view_line_index(size_t section, size_t line)
{
    return (int)(section * MAX_LINES + line - 1);
}

void view_line_id(char* id, size_t capacity, size_t section, size_t line)
{
    snprintf(id, capacity, "line-%zu-%zu", section, line);
}

Node view_route_link(Node parent,
                     const char* class_name,
                     const char* label,
                     const char* path,
                     AppAction action,
                     int index)
{
    Node link = element(parent, "a", class_name, label);
    dom_attr(link, "href", path);
    attribute_number(link, "data-action", action);
    attribute_number(link, "data-index", index);
    return link;
}
