add_executable(voxl-test "main.cpp" "HashTableTests.cpp" "tests.hpp" "tests.cpp" "TestModule.hpp" "TestModule.cpp")
target_link_libraries(voxl-test voxl-lib)
