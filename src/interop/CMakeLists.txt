onlyc_add_library(OnlyC.Interop Bridge.c Bridge.h "${ONLYC_GENERATED_DIR}/web_dom.c")
target_include_directories(OnlyC.Interop PUBLIC "${ONLYC_GENERATED_DIR}")
target_link_libraries(OnlyC.Interop PUBLIC OnlyC.EmliteHeaders PRIVATE emlitec::emlitec)
