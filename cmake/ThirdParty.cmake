include("${CMAKE_CURRENT_LIST_DIR}/GetCPM.cmake")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)

CPMAddPackage(
    NAME LibYAML
    URL https://codeload.github.com/yaml/libyaml/tar.gz/refs/tags/0.2.5
    URL_HASH SHA256=fa240dbf262be053f3898006d502d514936c818e422afdcf33921c63bed9bf2e
    OPTIONS "BUILD_TESTING OFF"
)
CPMAddPackage(
    NAME MD4C
    URL https://codeload.github.com/mity/md4c/tar.gz/refs/tags/release-0.5.2
    URL_HASH SHA256=55d0111d48fb11883aaee91465e642b8b640775a4d6993c2d0e7a8092758ef21
    OPTIONS "BUILD_MD2HTML_EXECUTABLE OFF"
)
CPMAddPackage(
    NAME emcore
    URL https://codeload.github.com/emlite/emcore/tar.gz/71117afa511daa737fe27b2583cf1fb8bc25b213
    DOWNLOAD_ONLY YES
)
CPMAddPackage(
    NAME emlitec
    URL https://codeload.github.com/emlite/emlitec/tar.gz/e9dcda3c9f764e77bfb3c13bfe296185de9d342c
    DOWNLOAD_ONLY YES
)
add_library(OnlyC.EmliteHeaders INTERFACE)
target_include_directories(
    OnlyC.EmliteHeaders
    SYSTEM
    INTERFACE
        "${emcore_SOURCE_DIR}/include"
        "${emlitec_SOURCE_DIR}/include"
)
if(ONLYC_WASM)
    add_subdirectory("${emcore_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/emcore" SYSTEM)
    # Use the upstream find_package entry point with an already-created target.
    # No submodule checkout, vendoring or downstream patch is necessary.
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/emcore-config.cmake" "set(emcore_FOUND TRUE)\n")
    add_subdirectory("${emlitec_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/emlitec" SYSTEM)
    return()
endif()

CPMAddPackage(
    NAME yyjson
    URL https://codeload.github.com/ibireme/yyjson/tar.gz/refs/tags/0.13.0
    OPTIONS
        "YYJSON_BUILD_TESTS OFF"
        "YYJSON_BUILD_MISC OFF"
)
CPMAddPackage(
    NAME LibXml2
    URL https://download.gnome.org/sources/libxml2/2.15/libxml2-2.15.4.tar.xz
    URL_HASH SHA256=98087fd181d9070724f3fbc65c7377db03038eb92bd882374daff44940138821
    OPTIONS
        "LIBXML2_WITH_PROGRAMS OFF"
        "LIBXML2_WITH_TESTS OFF"
        "LIBXML2_WITH_PYTHON OFF"
        "LIBXML2_WITH_ICONV OFF"
        "LIBXML2_WITH_ICU OFF"
        "LIBXML2_WITH_THREADS OFF"
        "LIBXML2_WITH_MODULES OFF"
        "LIBXML2_WITH_ZLIB OFF"
)
CPMAddPackage(
    NAME emlitejs
    URL https://codeload.github.com/emlite/emlite-js/tar.gz/b6930a95e9f818dbefeb307aa1e6c4751b719d5d
    DOWNLOAD_ONLY YES
)
set(ONLYC_WEBREF_REVISION 5b808095618fc298518e4ab8a1130cdb5d0ed193)
set(ONLYC_WEBREF_FILES)
foreach(SPEC dom html cssom-1 cssom-view-1 FileAPI url uievents pointerevents4)
    string(MAKE_C_IDENTIFIER "Webref_${SPEC}" PACKAGE)
    CPMAddPackage(
        NAME ${PACKAGE}
        URL
            "https://raw.githubusercontent.com/w3c/webref/${ONLYC_WEBREF_REVISION}/ed/idlparsed/${SPEC}.json"
        DOWNLOAD_ONLY
            YES
            DOWNLOAD_NO_EXTRACT
            YES
    )
    list(APPEND ONLYC_WEBREF_FILES "${${PACKAGE}_SOURCE_DIR}/${SPEC}.json")
endforeach()
if(ONLYC_BUILD_WEB)
    set(ONLYC_WASI_SYSROOT "" CACHE PATH "Optional pre-fetched WASI SDK 27 libc sysroot")
    if(NOT ONLYC_WASI_SYSROOT)
        CPMAddPackage(
            NAME WasiSysroot
            URL
                https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-27/wasi-sysroot-27.0.tar.gz
            URL_HASH SHA256=7110ac48f5d0b1f6ab67d57aecf52450540dddd790cafdc45f0fdfb429bdab84
            DOWNLOAD_ONLY YES
        )
        set(ONLYC_WASI_SYSROOT "${WasiSysroot_SOURCE_DIR}")
    endif()
endif()
if(ONLYC_BUILD_TESTS)
    CPMAddPackage(
        NAME GoogleTest
        URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
        OPTIONS
            "INSTALL_GTEST OFF"
            "BUILD_GMOCK OFF"
    )
endif()
