onlyc_add_test_executable(
    OnlyC.Core.Tests
    Given_Song.cpp
    Given_Artwork.cpp
    Given_LyricLink.cpp
    ../fakes/DomMock.cpp
)
target_link_libraries(OnlyC.Core.Tests PRIVATE OnlyC.Core OnlyC.Artwork OnlyC.App.Ui)
target_compile_definitions(
    OnlyC.Core.Tests
    PRIVATE ONLYC_TEST_YAML_LINE_WIDTH=${ONLYC_YAML_LINE_WIDTH}
)
