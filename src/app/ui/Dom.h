#ifndef ONLYC_APP_UI_DOM_H
#define ONLYC_APP_UI_DOM_H

#include "platform/web/Browser.h"

Node element(Node parent, const char* tag, const char* class_name, const char* text);
void attribute_number(Node node, const char* name, int value);
int dom_markdown(Node parent, const char* source);
#endif
