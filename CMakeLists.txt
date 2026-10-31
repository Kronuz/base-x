cmake_minimum_required(VERSION 3.14)
project(base_x LANGUAGES CXX)

# base_x.hh includes "uinteger_t.hh" (it uses arbitrary-precision integers for
# base conversion). Depend on the uinteger_t library directly instead of
# vendoring a copy. Pinned to a uinteger_t commit (bump it with API changes); for
# offline/hermetic builds set FETCHCONTENT_SOURCE_DIR_UINTEGER_T to a local
# checkout.
include(FetchContent)
FetchContent_Declare(uinteger_t
  GIT_REPOSITORY https://github.com/Kronuz/uinteger_t.git
  GIT_TAG        57ea9d584db2a24c2d9eddd6522586f796ab3e77)
FetchContent_MakeAvailable(uinteger_t)

add_library(base_x INTERFACE)
add_library(base_x::base_x ALIAS base_x)
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

  # The same test on the scalar code (the table-lookup paths are off), so both answer to its model.
  add_executable(base_x_test_scalar test/test.cc)
  target_link_libraries(base_x_test_scalar PRIVATE base_x)
  target_compile_definitions(base_x_test_scalar PRIVATE BASEX_NO_SIMD)
  add_test(NAME base_x_scalar COMMAND base_x_test_scalar)

  # A fixed-seed run of the decode fuzz target (test/fuzz_decode.cc builds for libFuzzer without the define).
  add_executable(base_x_fuzz_decode test/fuzz_decode.cc)
  target_link_libraries(base_x_fuzz_decode PRIVATE base_x)
  target_compile_definitions(base_x_fuzz_decode PRIVATE FUZZ_STANDALONE)
  add_test(NAME base_x_fuzz_decode COMMAND base_x_fuzz_decode 20000)

  # A runnable tour of the library (not a test): build & run ./build/base_x_demo
  add_executable(base_x_demo examples/demo.cc)
  target_link_libraries(base_x_demo PRIVATE base_x)
endif()
