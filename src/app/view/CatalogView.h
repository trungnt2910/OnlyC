#ifndef ONLYC_APP_VIEW_CATALOGVIEW_H
#define ONLYC_APP_VIEW_CATALOGVIEW_H

#include "platform/web/Browser.h"

typedef struct
{
    void* context;
    void (*mount)(void* context, Node parent);
    void (*unmount)(void* context);
} CatalogView;

#endif
