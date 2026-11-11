onlyc_add_test_executable(OnlyC.WebIdl.Tests Given_IdlGenerator.cpp)
target_link_libraries(OnlyC.WebIdl.Tests PRIVATE OnlyC.WebIdl.Generator)
