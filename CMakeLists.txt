cmake_minimum_required(VERSION 3.24)

project(
  ran_scheduler_simulator
  VERSION 0.1.0
  DESCRIPTION "Simplified educational 5G RAN scheduling simulator"
  LANGUAGES CXX)

option(RAN_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

include(CTest)
include(FetchContent)
include(cmake/ProjectOptions.cmake)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE)
set(JSON_BuildTests
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

add_library(
  ran_core STATIC
  src/channel_model.cpp
  src/config_loader.cpp
  src/packet.cpp
  src/project_info.cpp
  src/traffic_generator.cpp
  src/user_equipment.cpp)
add_library(ran::core ALIAS ran_core)
target_include_directories(ran_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(ran_core PRIVATE nlohmann_json::nlohmann_json)
target_compile_features(ran_core PUBLIC cxx_std_20)
set_target_properties(ran_core PROPERTIES CXX_EXTENSIONS OFF)
ran_apply_project_warnings(ran_core)
ran_apply_sanitizers(ran_core)

add_executable(ran_scheduler src/main.cpp)
target_link_libraries(ran_scheduler PRIVATE ran::core)
target_compile_features(ran_scheduler PRIVATE cxx_std_20)
set_target_properties(ran_scheduler PROPERTIES CXX_EXTENSIONS OFF)
ran_apply_project_warnings(ran_scheduler)
ran_apply_sanitizers(ran_scheduler)

if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7)
  set(gtest_force_shared_crt
      ON
      CACHE BOOL "" FORCE)
  set(BUILD_GMOCK
      OFF
      CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(
    ran_unit_tests
    tests/channel_model_test.cpp
    tests/config_loader_test.cpp
    tests/packet_test.cpp
    tests/project_info_test.cpp
    tests/traffic_generator_test.cpp
    tests/user_equipment_test.cpp)
  target_link_libraries(ran_unit_tests PRIVATE ran::core GTest::gtest_main
                                               nlohmann_json::nlohmann_json)
  target_compile_features(ran_unit_tests PRIVATE cxx_std_20)
  set_target_properties(ran_unit_tests PROPERTIES CXX_EXTENSIONS OFF)
  ran_apply_project_warnings(ran_unit_tests)
  ran_apply_sanitizers(ran_unit_tests)

  include(GoogleTest)
  gtest_discover_tests(ran_unit_tests)
endif()
