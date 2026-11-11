add_library(OnlyC.App.View INTERFACE)
target_sources(
    OnlyC.App.View
    INTERFACE
        ApplicationView.h
        ApplicationEvents.h
        CatalogView.h
        EditorView.h
        ReaderView.h
)
target_include_directories(OnlyC.App.View INTERFACE "${PROJECT_SOURCE_DIR}/src")

onlyc_add_library(
    OnlyC.App.View.Impl
    impl/ApplicationViewImpl.c
    impl/ApplicationViewImpl.h
    impl/ApplicationEvents.c
    impl/Controls.c
    impl/Controls.h
    impl/CatalogViewImpl.c
    impl/CatalogViewImpl.h
    impl/EditorViewImpl.c
    impl/EditorViewImpl.h
    impl/ReaderViewImpl.c
    impl/ReaderViewImpl.h
)
target_link_libraries(
    OnlyC.App.View.Impl
    PUBLIC
        OnlyC.App.View
        OnlyC.App.Controller
    PRIVATE OnlyC.App.Ui
)
