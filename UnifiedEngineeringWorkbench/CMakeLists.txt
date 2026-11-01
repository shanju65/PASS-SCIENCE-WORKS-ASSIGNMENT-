cmake_minimum_required(VERSION 3.16)
project(UnifiedEngineeringWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

# Core modules
set(UEW_CORE_SOURCES
  src/core/thermal_solver.cpp
  src/geometry/stl_parser.cpp
  src/geometry/csv_parser.cpp
  src/performance/multicore_engine.cpp
  src/hardware/hardware_manager.cpp
  src/neural/backprop_engine.cpp
  src/api/http_server.cpp
  src/api/routes.cpp
)

# Main executable (Launch.exe)
add_executable(Launch
  src/backend/main.cpp
  ${UEW_CORE_SOURCES}
)
target_include_directories(Launch PRIVATE src)
if(WIN32)
  target_link_libraries(Launch PRIVATE ws2_32 user32 shell32 advapi32)
endif()

# Unit test runner
add_executable(workbench_tests
  tests/test_runner.cpp
  ${UEW_CORE_SOURCES}
)
target_include_directories(workbench_tests PRIVATE src)
if(WIN32)
  target_link_libraries(workbench_tests PRIVATE ws2_32 advapi32)
endif()

# Copy Launch executable to root folder as Launch.exe
add_custom_command(TARGET Launch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Launch> ${CMAKE_SOURCE_DIR}/Launch.exe
)
