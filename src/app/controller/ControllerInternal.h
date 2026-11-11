#ifndef ONLYC_APP_CONTROLLER_CONTROLLERINTERNAL_H
#define ONLYC_APP_CONTROLLER_CONTROLLERINTERNAL_H

#include "app/controller/ApplicationController.h"

int controller_load_published_song(ApplicationController* controller, int index);
void controller_refresh_dirty(ApplicationController* controller);
void controller_present(ApplicationController* controller, ApplicationChange change);
int controller_prepare_artwork(ApplicationController* controller);
void controller_fill_artwork_color(ApplicationController* controller);
int controller_commit_editor(ApplicationController* controller);
void controller_navigate(ApplicationController* controller, const char* path);
void controller_selection_url(ApplicationController* controller);
void controller_export_song(ApplicationController* controller);
#endif
