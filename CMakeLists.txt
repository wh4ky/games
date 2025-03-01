# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(Games VERSION 0.0.0)

# C/CMake stuff
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED true)

# Build type options.
if(CMAKE_BUILD_TYPE)
  string(TOLOWER ${CMAKE_BUILD_TYPE} BUILDTYPE_LOWER)

  if(BUILDTYPE_LOWER STREQUAL "debug")
    add_compile_options(
      -g3
      -Werror
      -Wall
      -Wextra
      -Wconversion
      -Wpedantic
      -Wunused-result
      -Wdouble-promotion
      -Wnull-dereference)
  elseif(BUILDTYPE_LOWER STREQUAL "release")
    add_compile_options(-O3 -DNDEBUG)
  endif()

endif()

# Compile all games

set(GAMES_LIST snake)

foreach(game ${GAMES_LIST})
  add_executable(${game} src/${game}.c)
  target_compile_options(${game} PRIVATE ${CMAKE_C_FLAGS})
  set_target_properties(${game} PROPERTIES OUTPUT_NAME ${game})
endforeach()
