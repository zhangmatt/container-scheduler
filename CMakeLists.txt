cmake_minimum_required(VERSION 3.20)

project(mini_borg
  VERSION 0.1.0
  DESCRIPTION "A deterministic Borg-style container scheduler in modern C++"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mini_borg_core
  src/agent/health.cpp
  src/agent/node_agent.cpp
  src/agent/runtime.cpp
  src/common/model.cpp
  src/common/resources.cpp
  src/control_plane/api_server.cpp
  src/control_plane/reconciler.cpp
  src/control_plane/scheduler.cpp
  src/control_plane/state_store.cpp)

target_include_directories(mini_borg_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
  target_compile_options(mini_borg_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(mini_borg_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(control-plane src/server/control_main.cpp)
target_link_libraries(control-plane PRIVATE mini_borg_core)

add_executable(node-agent src/server/agent_main.cpp)
target_link_libraries(node-agent PRIVATE mini_borg_core)

add_executable(borgctl src/cli/borgctl.cpp)
target_link_libraries(borgctl PRIVATE mini_borg_core)

add_executable(mini_borg_tests
  test/test_main.cpp
  test/scheduler_test.cpp
  test/sim_test.cpp)
target_include_directories(mini_borg_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/test)
target_link_libraries(mini_borg_tests PRIVATE mini_borg_core)

enable_testing()
add_test(NAME mini_borg_tests COMMAND mini_borg_tests)
