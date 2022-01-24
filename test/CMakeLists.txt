add_executable(
	voxl-test
 "Lang/Parsing/ScannerTest.cpp")

enable_testing()
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

target_link_libraries(voxl-test gtest gtest_main)
target_link_libraries(voxl-test voxl-lib)

add_test(voxl-test voxl-test)