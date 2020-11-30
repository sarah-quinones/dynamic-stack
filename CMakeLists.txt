cmake_minimum_required(VERSION 3.16)
project(dynamic-stack CXX)

get_directory_property(has_parent PARENT_DIRECTORY)
if(has_parent)
  set(top_level FALSE)
else()
  set(top_level TRUE)
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if(top_level AND ENABLE_TESTING)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(cmake/compiler_warnings.cmake)
include(cmake/static_analyzers.cmake)
include(cmake/sanitizers.cmake)
include(cmake/conan.cmake)

add_library(dynamic-stack INTERFACE)
target_include_directories(
  dynamic-stack INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(top_level AND ENABLE_TESTING)
  # Conan dependencies
  set(CONAN_REQUIRES # MIT License
      gsl-lite/0.37.0 fmt/7.1.2 backward-cpp/1.5
  )
  target_compile_definitions(
    dynamic-stack INTERFACE gsl_CONFIG_DEFAULTS_VERSION=1
  )
  set(CONAN_OPTIONS)
  run_conan()

  target_link_libraries(
    dynamic-stack INTERFACE CONAN_PKG::gsl-lite CONAN_PKG::fmt
  )

  add_library(project_warnings INTERFACE)
  add_library(project_options INTERFACE)

  target_link_libraries(
    dynamic-stack INTERFACE project_options project_warnings
  )

  include(cmake/extra_local_settings.cmake)

  set_project_warnings(project_warnings)
  enable_sanitizers(project_options)

  enable_testing()
  add_subdirectory(test)
endif()
