#ifndef ONLYC_APP_VIEW_IMPL_CATALOGVIEWIMPL_H
#define ONLYC_APP_VIEW_IMPL_CATALOGVIEWIMPL_H

#include "app/controller/ApplicationController.h"
#include "app/view/CatalogView.h"

typedef struct
{
    const ApplicationState* state;
    ApplicationController* controller;
    ApplicationSubscription subscription;
    Node search;
    Node results;
} CatalogViewImpl;

void catalog_view_init(CatalogViewImpl* view, ApplicationController* controller);
void catalog_view_destroy(CatalogViewImpl* view);
CatalogView catalog_view_port(CatalogViewImpl* view);
void catalog_view_input(CatalogViewImpl* view);

#endif
