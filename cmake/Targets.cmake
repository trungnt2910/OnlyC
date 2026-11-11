function(_onlyc_configure_target target)
    set_target_properties(${target} PROPERTIES C_EXTENSIONS OFF)
    target_include_directories(
        ${target}
        PUBLIC "${PROJECT_SOURCE_DIR}/include"
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
            "${PROJECT_SOURCE_DIR}/tools"
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(ONLYC_SANITIZE AND NOT ONLYC_WASM)
        target_compile_options(
            ${target}
            PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endif()
endfunction()

function(onlyc_add_library target)
    add_library(${target} STATIC ${ARGN})
    _onlyc_configure_target(${target})
endfunction()

function(onlyc_add_executable target)
    add_executable(${target} ${ARGN})
    _onlyc_configure_target(${target})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
endfunction()
