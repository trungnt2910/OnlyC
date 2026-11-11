onlyc_add_library(OnlyC.App.Model ApplicationState.c ApplicationState.h EditorState.c EditorState.h)
target_link_libraries(OnlyC.App.Model PUBLIC OnlyC.Core)
