# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(Games VERSION 0.0.0)

# C/CMake stuff
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED true)

# Definitions
set(OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")
file(MAKE_DIRECTORY ${OUTPUT_DIR})

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
set(GAMES_LIST snake minesweeper)

foreach(game ${GAMES_LIST})
  add_executable(${game} ${CMAKE_SOURCE_DIR}/src/${game}.c)
  target_compile_options(${game} PRIVATE ${CMAKE_C_FLAGS})
  set_target_properties(
    ${game} PROPERTIES OUTPUT_NAME ${game} RUNTIME_OUTPUT_DIRECTORY
                                           ${OUTPUT_DIR}/bin)
endforeach()

# Put script in /out
set(PROGRAM_ARRAY "snake" "other" "xyz")
string(REPLACE ";" " " PROGRAM_ARRAY_STRING "${GAMES_LIST}")

configure_file(${CMAKE_SOURCE_DIR}/src/launcher.sh.in
               ${CMAKE_BINARY_DIR}/tmp/launcher @ONLY)

file(
  COPY ${CMAKE_BINARY_DIR}/tmp/launcher
  DESTINATION ${OUTPUT_DIR}
  FILE_PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE)
