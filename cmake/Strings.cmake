include_guard(GLOBAL)

# Collect symbolic C strings in the caller's directory scope. Independent
# catalogs can share these helpers without sharing their declarations.
function(onlyc_resource_string key keyword value)
    if(NOT ARGC EQUAL 3 OR NOT keyword STREQUAL "VALUE" OR NOT key MATCHES "^[A-Z][A-Z0-9_]*$")
        message(FATAL_ERROR "onlyc_resource_string requires an uppercase key and VALUE.")
    endif()
    if(DEFINED ONLYC_STRING_${key})
        set(value "${ONLYC_STRING_${key}}")
    endif()
    if(key IN_LIST ONLYC_RESOURCE_KEYS)
        message(FATAL_ERROR "Duplicate UI string '${key}'.")
    endif()
    list(APPEND ONLYC_RESOURCE_KEYS "${key}")
    set(ONLYC_RESOURCE_KEYS "${ONLYC_RESOURCE_KEYS}" PARENT_SCOPE)

    # Escape C literals without changing Unicode or interpreting percent signs.
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\r" "\\r" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    string(REPLACE "?" "\\?" value "${value}")
    string(APPEND ONLYC_RESOURCE_DECLARATIONS "#define ONLYC_STRING_${key} \"${value}\"\n")
    set(ONLYC_RESOURCE_DECLARATIONS "${ONLYC_RESOURCE_DECLARATIONS}" PARENT_SCOPE)
endfunction()

function(onlyc_generate_resources output)
    file(
        CONFIGURE
        OUTPUT "${output}"
        CONTENT
            [=[/* Generated string constants. Do not edit. */
#pragma once

@ONLYC_RESOURCE_DECLARATIONS@
]=]
        @ONLY
        NEWLINE_STYLE UNIX
    )
endfunction()
