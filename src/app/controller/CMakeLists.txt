onlyc_add_library(
    OnlyC.App.Controller
    ApplicationController.c
    ApplicationController.h
    ApplicationServices.h
    ControllerInternal.h
    EditorController.c
    FileImportController.c
    NavigationController.c
    ArtworkController.c
)
target_link_libraries(OnlyC.App.Controller PUBLIC OnlyC.App.Model OnlyC.App.View OnlyC.App.Strings)
