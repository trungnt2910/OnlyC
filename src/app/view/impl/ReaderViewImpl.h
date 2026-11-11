#ifndef ONLYC_APP_VIEW_IMPL_READERVIEWIMPL_H
#define ONLYC_APP_VIEW_IMPL_READERVIEWIMPL_H

#include "app/controller/ApplicationController.h"
#include "app/view/EditorView.h"
#include "app/view/ReaderView.h"

typedef struct
{
    const ApplicationState* state;
    ApplicationController* controller;
    ApplicationSubscription subscription;
    EditorView editor;
    Node grid;
    Node title_panel_host;
} ReaderViewImpl;

void reader_view_init(ReaderViewImpl* view, ApplicationController* controller, EditorView editor);
void reader_view_destroy(ReaderViewImpl* view);
ReaderView reader_view_port(ReaderViewImpl* view);
void reader_view_key(ReaderViewImpl* view);

#endif
