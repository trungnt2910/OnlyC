file(MAKE_DIRECTORY "${ONLYC_GENERATED_DIR}" "${ONLYC_SITE_DIR}")
add_custom_command(
    OUTPUT
        "${ONLYC_GENERATED_DIR}/web_dom.h"
        "${ONLYC_GENERATED_DIR}/web_dom.c"
        "${ONLYC_GENERATED_DIR}/coverage.tsv"
    COMMAND
        OnlyC.IdlGen
        "${ONLYC_GENERATED_DIR}"
        ${ONLYC_WEBREF_FILES}
    DEPENDS
        OnlyC.IdlGen
        ${ONLYC_WEBREF_FILES}
    VERBATIM
)
add_custom_target(
    bindings
    ALL
    DEPENDS
        "${ONLYC_GENERATED_DIR}/web_dom.h"
        "${ONLYC_GENERATED_DIR}/web_dom.c"
        "${ONLYC_GENERATED_DIR}/coverage.tsv"
)

file(GLOB ONLYC_SONG_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/songs/*.yaml")
if(NOT ONLYC_SONG_FILES)
    message(FATAL_ERROR "Add at least one song to songs/")
endif()
set(ONLYC_PREVIEW_PAGES)
foreach(SONG IN LISTS ONLYC_SONG_FILES)
    get_filename_component(SLUG "${SONG}" NAME_WE)
    file(MAKE_DIRECTORY "${ONLYC_SITE_DIR}/s/${SLUG}")
    list(
        APPEND
        ONLYC_PREVIEW_PAGES
        "${ONLYC_SITE_DIR}/${SLUG}.html"
        "${ONLYC_SITE_DIR}/s/${SLUG}/index.html"
    )
endforeach()
add_custom_command(
    OUTPUT
        "${ONLYC_GENERATED_DIR}/embedded.h"
        "${ONLYC_SITE_DIR}/index.html"
        ${ONLYC_PREVIEW_PAGES}
    COMMAND
        OnlyC.SongPack
        "${ONLYC_SITE_DIR}"
        "${ONLYC_GENERATED_DIR}"
        "${ONLYC_SITE_ORIGIN}"
        ${ONLYC_SONG_FILES}
    DEPENDS
        OnlyC.SongPack
        ${ONLYC_SONG_FILES}
    VERBATIM
)
add_custom_target(
    site
    ALL
    DEPENDS
        "${ONLYC_GENERATED_DIR}/embedded.h"
        "${ONLYC_SITE_DIR}/index.html"
        ${ONLYC_PREVIEW_PAGES}
)
configure_file("${emlitejs_SOURCE_DIR}/src/emlite.js" "${ONLYC_SITE_DIR}/emlite.js" COPYONLY)

if(ONLYC_BUILD_WEB)
    include(ExternalProject)
    # The current libc uses the WASI ABI; browser I/O uses the C browser bindings.
    ExternalProject_Add(
        OnlyC.Wasm
        SOURCE_DIR "${PROJECT_SOURCE_DIR}"
        BINARY_DIR "${PROJECT_BINARY_DIR}/wasm"
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_ARGS
            -DCMAKE_SYSTEM_NAME=WASI
            -DCMAKE_SYSTEM_PROCESSOR=wasm32
            -DCMAKE_C_COMPILER_TARGET=wasm32-wasip1
            -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
            "-DCMAKE_C_FLAGS_INIT=-ffunction-sections -fdata-sections"
            -DCMAKE_SYSROOT=${ONLYC_WASI_SYSROOT}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_BUILD_TYPE=MinSizeRel
            -DONLYC_GENERATED_DIR=${ONLYC_GENERATED_DIR}
            -DONLYC_SITE_DIR=${ONLYC_SITE_DIR}
            -DONLYC_YAML_LINE_WIDTH=${ONLYC_YAML_LINE_WIDTH}
            -DCPM_SOURCE_CACHE=${CPM_SOURCE_CACHE}
            ${ONLYC_WASM_CMAKE_ARGS}
        BUILD_COMMAND
            ${CMAKE_COMMAND}
            --build
            <BINARY_DIR>
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
        BUILD_BYPRODUCTS "${ONLYC_SITE_DIR}/app.wasm"
        DEPENDS
            bindings
            site
    )
    add_custom_target(web DEPENDS OnlyC.Wasm)
endif()
