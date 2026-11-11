#ifndef ONLYC_APP_VIEW_IMPL_EDITORVIEWIMPL_H
#define ONLYC_APP_VIEW_IMPL_EDITORVIEWIMPL_H

#include "app/controller/ApplicationController.h"
#include "app/view/EditorView.h"

typedef struct
{
    const ApplicationState* state;
    ApplicationController* controller;
    ApplicationSubscription subscription;
    Node fields[EDITOR_FIELD_COUNT];
    Node preview;
    Node link_labels[MAX_LINKS];
    Node link_urls[MAX_LINKS];
} EditorViewImpl;

void editor_view_init(EditorViewImpl* view, ApplicationController* controller);
void editor_view_destroy(EditorViewImpl* view);
EditorView editor_view_port(EditorViewImpl* view);
void editor_view_input(EditorViewImpl* view, const char* id);

#endif
