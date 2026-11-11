set(ONLYC_YAML_LINE_WIDTH 100 CACHE STRING "Column limit for exported YAML annotations")
if(NOT ONLYC_YAML_LINE_WIDTH MATCHES "^[1-9][0-9]*$"
   OR ONLYC_YAML_LINE_WIDTH LESS 5 OR ONLYC_YAML_LINE_WIDTH GREATER 2147483647
)
    message(FATAL_ERROR "ONLYC_YAML_LINE_WIDTH must be an integer from 5 to 2147483647.")
endif()

onlyc_add_library(
    OnlyC.Core
    Buffer.c
    LyricLink.c
    LyricLink.h
    Lyrics.c
    Song.c
    SongYaml.c
    Song.h
    Color.c
    Color.h
)
target_link_libraries(OnlyC.Core PRIVATE yaml)
target_compile_definitions(OnlyC.Core PRIVATE ONLYC_YAML_LINE_WIDTH=${ONLYC_YAML_LINE_WIDTH})
onlyc_add_library(OnlyC.Artwork Artwork.c Artwork.h)
target_link_libraries(OnlyC.Artwork PUBLIC OnlyC.Core)
