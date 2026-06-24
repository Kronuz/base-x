cmake_minimum_required(VERSION 3.14)
project(base_x LANGUAGES CXX)

add_library(base_x INTERFACE)
target_include_directories(base_x INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(base_x INTERFACE cxx_std_17)

# Only build the smoke test when this repo is the top-level project, so
# consumers (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(base_x_test test/test.cc)
  target_link_libraries(base_x_test PRIVATE base_x)
  add_test(NAME base_x COMMAND base_x_test)
endif()
