onlyc_add_library(OnlyC.SongPack.Html PreviewPage.c PreviewPage.h)
target_link_libraries(
    OnlyC.SongPack.Html
    PUBLIC OnlyC.Core
    PRIVATE
        LibXml2::LibXml2
        OnlyC.App.Strings
)
onlyc_add_executable(OnlyC.SongPack Main.c)
target_link_libraries(OnlyC.SongPack PRIVATE OnlyC.SongPack.Html)
set_target_properties(OnlyC.SongPack PROPERTIES OUTPUT_NAME songpack)
