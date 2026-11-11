#ifndef ONLYC_APP_CONTROLLER_APPLICATIONSERVICES_H
#define ONLYC_APP_CONTROLLER_APPLICATIONSERVICES_H

#include "core/Song.h"

/* Browser I/O is injected, so controllers have no DOM or WASM dependency. */
typedef struct
{
    void* context;
    int (*location)(void* context, char* path, int capacity);
    void (*navigate)(void* context, const char* path);
    int (*confirm)(void* context, const char* question);
    int (*download)(void* context, const char* name, const Buffer* bytes);
    void (*select_file)(void* context);
    void (*artwork)(void* context, const char* url, unsigned request);
} ApplicationServices;
#endif
