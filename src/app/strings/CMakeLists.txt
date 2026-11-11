# The native configure generates one header, shared by the native tools and WASM.
# Cross builds consume it without replacing native cache overrides with defaults.
if(NOT ONLYC_WASM)
    include(Resources.cmake)
    onlyc_generate_resources("${ONLYC_GENERATED_DIR}/strings/Resources.h")
endif()

add_library(OnlyC.App.Strings INTERFACE)
target_include_directories(OnlyC.App.Strings INTERFACE "${ONLYC_GENERATED_DIR}")
