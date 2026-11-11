#ifndef ONLYC_APP_CONTROLLER_APPLICATIONCONTROLLER_H
#define ONLYC_APP_CONTROLLER_APPLICATIONCONTROLLER_H

#include <stddef.h>

#include "app/controller/ApplicationServices.h"
#include "app/model/ApplicationState.h"
#include "app/view/ApplicationView.h"

typedef enum
{
    APP_SELECT_SONG = 1,
    APP_TOGGLE_EDIT,
    APP_CLOSE_PANEL,
    APP_APPLY,
    APP_IMPORT,
    APP_EXPORT,
    APP_SELECT_LINE,
    APP_ADD_SECTION,
    APP_DELETE_SECTION,
    APP_REMOVE_ANNOTATION,
    APP_HOME,
    APP_SELECT_TITLE,
    APP_ADD_LINK,
    APP_REMOVE_LINK,
    APP_RESUME_DRAFT
} AppAction;

typedef struct
{
    ApplicationState* state;
    ApplicationView view;
    ApplicationServices services;
} ApplicationController;

void application_controller_init(ApplicationController* controller,
                                 ApplicationState* state,
                                 ApplicationView view,
                                 ApplicationServices services);
void application_controller_start(ApplicationController* controller);
void application_controller_action(ApplicationController* controller, int action, int index);
/* Values are copied into editor state. NULL records an input-read failure and
 * prevents committing stale text until that field receives a readable value. */
void application_controller_changed(ApplicationController* controller,
                                    EditorField field,
                                    const char* value);
void application_controller_link_changed(ApplicationController* controller,
                                         size_t index,
                                         int url,
                                         const char* value);
void application_controller_import(ApplicationController* controller,
                                   const char* text,
                                   size_t length);
void application_controller_location(ApplicationController* controller);
void application_controller_search(ApplicationController* controller, const char* query);
void application_controller_file_error(ApplicationController* controller);
void application_controller_artwork_ready(ApplicationController* controller,
                                          unsigned request,
                                          const char* color);
int application_controller_request_file(ApplicationController* controller,
                                        const char* name,
                                        int size,
                                        int count);

#endif
