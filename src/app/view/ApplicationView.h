#ifndef ONLYC_APP_VIEW_APPLICATIONVIEW_H
#define ONLYC_APP_VIEW_APPLICATIONVIEW_H

#include <stddef.h>

typedef enum
{
    VIEW_FOCUS_PANEL,
    VIEW_FOCUS_TITLE,
    VIEW_FOCUS_LINE,
    VIEW_FOCUS_LINE_AND_SCROLL,
    VIEW_FOCUS_TITLE_AND_SCROLL
} ViewFocus;

/* Controllers request presentation effects without knowing browser objects.
 * Input values arrive in controller methods and are owned by model state. */
typedef struct
{
    void* context;
    void (*focus)(void* context, ViewFocus target, size_t section, size_t line);
} ApplicationView;
#endif
