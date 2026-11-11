#ifndef ONLYC_APP_VIEW_EDITORVIEW_H
#define ONLYC_APP_VIEW_EDITORVIEW_H

#include "platform/web/Browser.h"

typedef struct
{
    void* context;
    void (*mount_metadata)(void* context, Node parent);
    void (*mount_annotation)(void* context, Node parent);
    void (*unmount_annotation)(void* context);
    void (*unmount)(void* context);
} EditorView;

#endif
