onlyc_add_library(OnlyC.App.Ui Dom.c Dom.h Theme.c Theme.h)
target_link_libraries(OnlyC.App.Ui PRIVATE OnlyC.Core md4c-html)
target_include_directories(OnlyC.App.Ui SYSTEM PRIVATE "${MD4C_SOURCE_DIR}/src")
