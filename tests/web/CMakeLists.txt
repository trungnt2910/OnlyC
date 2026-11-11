onlyc_add_test_executable(
    OnlyC.Browser.Tests
    Given_Browser.cpp
    ../../src/platform/web/Browser.c
    ../../src/platform/web/Discussion.c
    ../../src/interop/Bridge.c
    "${ONLYC_GENERATED_DIR}/web_dom.c"
)
target_include_directories(OnlyC.Browser.Tests PRIVATE "${ONLYC_GENERATED_DIR}")
target_link_libraries(OnlyC.Browser.Tests PRIVATE OnlyC.EmliteHeaders OnlyC.Artwork)
add_dependencies(OnlyC.Browser.Tests bindings)
onlyc_add_test_executable(OnlyC.Preview.Tests Given_PreviewPage.cpp)
target_link_libraries(OnlyC.Preview.Tests PRIVATE OnlyC.SongPack.Html)
if(ONLYC_BUILD_WEB)
    onlyc_add_test_executable(OnlyC.Artifact.Tests Given_StaticArtifacts.cpp)
    target_link_libraries(OnlyC.Artifact.Tests PRIVATE OnlyC.Core OnlyC.App.Strings)
    target_compile_definitions(
        OnlyC.Artifact.Tests
        PRIVATE
            SITE_DIR="${ONLYC_SITE_DIR}"
            SITE_ORIGIN="${ONLYC_SITE_ORIGIN}"
    )
    add_dependencies(OnlyC.Artifact.Tests OnlyC.Wasm)
endif()
