set(TEST_CATALOG_DIR "${CMAKE_CURRENT_BINARY_DIR}/catalog")
file(MAKE_DIRECTORY "${TEST_CATALOG_DIR}/s/amber" "${TEST_CATALOG_DIR}/s/frost")
add_custom_command(
    OUTPUT "${TEST_CATALOG_DIR}/TestCatalog.h"
    COMMAND
        OnlyC.SongPack
        "${TEST_CATALOG_DIR}"
        "${TEST_CATALOG_DIR}"
        "https://example.test"
        "${PROJECT_SOURCE_DIR}/tests/fixtures/amber.yaml"
        "${PROJECT_SOURCE_DIR}/tests/fixtures/frost.yaml"
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        copy_if_different
        "${TEST_CATALOG_DIR}/embedded.h"
        "${TEST_CATALOG_DIR}/TestCatalog.h"
    DEPENDS
        OnlyC.SongPack
        ../fixtures/amber.yaml
        ../fixtures/frost.yaml
    VERBATIM
)
add_custom_target(test_catalog DEPENDS "${TEST_CATALOG_DIR}/TestCatalog.h")

onlyc_add_test_executable(
    OnlyC.Application.Tests
    Given_Application.cpp
    Given_Strings.cpp
    ../fakes/DomMock.cpp
    ../../src/platform/web/Services.c
)
target_link_libraries(OnlyC.Application.Tests PRIVATE OnlyC.App.View.Impl OnlyC.Core)
target_include_directories(OnlyC.Application.Tests PRIVATE "${TEST_CATALOG_DIR}")
add_dependencies(OnlyC.Application.Tests test_catalog)

# Exercise the same C-literal generator with text which cannot be pasted into
# a quoted C string directly. This catalog is local to the test directory.
onlyc_resource_string(
    TEST_ESCAPES
    VALUE
    [=[Quotes: "text"; path: C:\songs\demo; @ONLYC_SITE_ORIGIN@; $<CONFIG>; ??/
Tiếng Việt	100%]=]
)
set(ONLYC_STRING_TEST_OVERRIDE "Override; \"quoted\" \\ value")
onlyc_resource_string(TEST_OVERRIDE VALUE "Default value")
onlyc_resource_string(TEST_EMPTY VALUE "")
onlyc_generate_resources("${CMAKE_CURRENT_BINARY_DIR}/StringsProbe.h")
target_include_directories(OnlyC.Application.Tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# This executable deliberately links no view, UI, browser or Emlite code.
onlyc_add_test_executable(
    OnlyC.Controller.Tests
    Given_ApplicationController.cpp
    Given_ApplicationState.cpp
    Given_EditorState.cpp
)
target_link_libraries(OnlyC.Controller.Tests PRIVATE OnlyC.App.Controller)
target_include_directories(OnlyC.Controller.Tests PRIVATE "${TEST_CATALOG_DIR}")
add_dependencies(OnlyC.Controller.Tests test_catalog)
