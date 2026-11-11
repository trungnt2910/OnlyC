#ifndef ONLYC_APP_VIEW_IMPL_APPLICATIONVIEWIMPL_H
#define ONLYC_APP_VIEW_IMPL_APPLICATIONVIEWIMPL_H

#include "app/controller/ApplicationController.h"
#include "app/view/ApplicationEvents.h"
#include "app/view/impl/CatalogViewImpl.h"
#include "app/view/impl/EditorViewImpl.h"
#include "app/view/impl/ReaderViewImpl.h"

typedef struct
{
    ApplicationController* controller;
    const ApplicationState* state;
    ApplicationSubscription subscription;
    CatalogViewImpl catalog_impl;
    ReaderViewImpl reader_impl;
    EditorViewImpl editor_impl;
    CatalogView catalog;
    ReaderView reader;
} BrowserApplicationView;

ApplicationView browser_view_port(BrowserApplicationView* view);
void browser_view_attach(BrowserApplicationView* view, ApplicationController* controller);
void browser_view_detach(BrowserApplicationView* view);
void browser_view_listen(void);
void browser_view_event(BrowserApplicationView* view, int code);

#endif
