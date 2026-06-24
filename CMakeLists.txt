cmake_minimum_required(VERSION 3.14)
project(base_x LANGUAGES CXX)

# base_x.hh includes "uinteger_t.hh" (it uses arbitrary-precision integers for
# base conversion). Depend on the uinteger_t library directly instead of
# vendoring a copy. Pinned to a commit SHA for reproducible builds; for
# offline/hermetic builds set FETCHCONTENT_SOURCE_DIR_UINTEGER_T to a local
# checkout.
include(FetchContent)
FetchContent_Declare(uinteger_t
  GIT_REPOSITORY https://github.com/Kronuz/uinteger_t.git
  GIT_TAG        a455f3317967d2c91e0d5c2064e47441dc04dcfb)
FetchContent_MakeAvailable(uinteger_t)

add_library(base_x INTERFACE)
target_include_directories(base_x INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(base_x INTERFACE cxx_std_17)
# Propagate the dependency so anything linking base_x can resolve "uinteger_t.hh".
target_link_libraries(base_x INTERFACE uinteger_t)

# Only build the smoke test when this repo is the top-level project, so
# consumers (e.g. via FetchContent) don't build our tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(base_x_test test/test.cc)
  target_link_libraries(base_x_test PRIVATE base_x)
  add_test(NAME base_x COMMAND base_x_test)
endif()
