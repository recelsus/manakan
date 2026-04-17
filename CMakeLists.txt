# vim: ft=cmake
cmake_minimum_required(VERSION 3.22)
project(manakan LANGUAGES CXX)

# Export compile_commands.json for LSP/clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MANAKAN_INSTALL_TO_HOME_LOCAL "Install under \$HOME/.local when using CMake install" ON)

if((CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR MANAKAN_INSTALL_TO_HOME_LOCAL))
  if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix" FORCE)
  else()
    message(FATAL_ERROR "HOME must be set to install under ~/.local")
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(manakan_core STATIC
  src/app/bootstrap.cpp
  src/app/run_command.cpp
  src/cli/cli.cpp
  src/config/config_loader.cpp
  src/resolve/request_resolver.cpp
  src/utils/helpers.cpp
)

target_include_directories(manakan_core PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}
)

add_executable(manakan
  src/main.cpp
  src/transport/http_client.cpp
)

target_link_libraries(manakan PRIVATE manakan_core)

# Prefer OpenSSL via CMake packages; define for cpp-httplib if available
find_package(OpenSSL QUIET)
if (OpenSSL_FOUND)
  target_link_libraries(manakan_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(manakan_core PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
else()
  message(STATUS "OpenSSL not found via CMake; attempting platform libs fallback")
  if (UNIX)
    target_link_libraries(manakan_core PUBLIC ssl crypto pthread)
  elseif (WIN32)
    target_link_libraries(manakan_core PUBLIC ws2_32)
  endif()
endif()

# Common runtime output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_target_properties(manakan PROPERTIES OUTPUT_NAME "manakan")

# Build metadata (optional, mirrors karing approach)
execute_process(
  COMMAND git -C ${CMAKE_SOURCE_DIR} describe --tags --always --dirty
  OUTPUT_VARIABLE MANAKAN_GIT_DESCRIBE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT MANAKAN_GIT_DESCRIBE)
  execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short=12 HEAD
    OUTPUT_VARIABLE MANAKAN_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
if(NOT MANAKAN_GIT_DESCRIBE)
  set(MANAKAN_GIT_DESCRIBE "unknown")
endif()
string(TIMESTAMP MANAKAN_BUILD_TIME_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)
target_compile_definitions(manakan PRIVATE
  MANAKAN_GIT_REV=\"${MANAKAN_GIT_DESCRIBE}\"
  MANAKAN_BUILD_TIME=\"${MANAKAN_BUILD_TIME_UTC}\"
  MANAKAN_CXX_STANDARD=${CMAKE_CXX_STANDARD}
  MANAKAN_BUILD_TYPE=\"${CMAKE_BUILD_TYPE}\"
)

target_compile_definitions(manakan_core PUBLIC
  MANAKAN_GIT_REV=\"${MANAKAN_GIT_DESCRIBE}\"
  MANAKAN_BUILD_TIME=\"${MANAKAN_BUILD_TIME_UTC}\"
  MANAKAN_CXX_STANDARD=${CMAKE_CXX_STANDARD}
  MANAKAN_BUILD_TYPE=\"${CMAKE_BUILD_TYPE}\"
)

# Create/refresh a top-level compile_commands.json symlink (or copy on Windows)
if (EXISTS ${CMAKE_BINARY_DIR}/compile_commands.json)
  if (WIN32)
    add_custom_target(link_compile_commands ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${CMAKE_BINARY_DIR}/compile_commands.json
              ${CMAKE_SOURCE_DIR}/compile_commands.json
      COMMENT "Copy compile_commands.json to project root for clangd")
  else()
    add_custom_target(link_compile_commands ALL
      COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/compile_commands.json
      COMMAND ${CMAKE_COMMAND} -E create_symlink
              ${CMAKE_BINARY_DIR}/compile_commands.json
              ${CMAKE_SOURCE_DIR}/compile_commands.json
      COMMENT "Symlink compile_commands.json to project root for clangd")
  endif()
endif()

enable_testing()

add_executable(manakan_tests
  tests/config_resolver_tests.cpp
)
target_link_libraries(manakan_tests PRIVATE manakan_core)
set_target_properties(manakan_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME manakan_tests COMMAND $<TARGET_FILE:manakan_tests>)

# Install rules
include(GNUInstallDirs)
install(TARGETS manakan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(DEFINED ENV{XDG_DATA_HOME} AND NOT "$ENV{XDG_DATA_HOME}" STREQUAL "")
  install(FILES completions/manakan.bash DESTINATION "$ENV{XDG_DATA_HOME}/bash_completion" RENAME manakan.bash)
endif()

# Clean convenience target
add_custom_target(clean-all
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
  COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
)
