#ifndef ONLYC_APP_VIEW_IMPL_CONTROLS_H
#define ONLYC_APP_VIEW_IMPL_CONTROLS_H

#include "app/controller/ApplicationController.h"
#include "app/ui/Dom.h"

Node view_button(
    Node parent, AppAction action, int index, const char* class_name, const char* label);
Node view_field(
    Node parent, const char* id, const char* label, const char* value, int multiline, int max);
int view_line_index(size_t section, size_t line);
void view_line_id(char* id, size_t capacity, size_t section, size_t line);
Node view_route_link(Node parent,
                     const char* class_name,
                     const char* label,
                     const char* path,
                     AppAction action,
                     int index);
#endif
