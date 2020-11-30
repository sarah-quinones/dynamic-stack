include(../cmake/doctest.cmake)

include_directories(include)

add_library(doctest_main STATIC src/doctest_main.cpp)
add_library(backward_cpp_main OBJECT src/backward.cpp)
target_link_libraries(backward_cpp_main CONAN_PKG::backward-cpp)
set(testlibs dynamic-stack backward_cpp_main doctest_main)

add_executable(test_main src/main.cpp)
target_link_libraries(test_main ${testlibs})
