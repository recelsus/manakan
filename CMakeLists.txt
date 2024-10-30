# vim: ft=cmake
cmake_minimum_required(VERSION 3.10)

project(manakan)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(PLATFORM "Linux_ARM")
    else()
        set(PLATFORM "Linux_x86")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPHTTPLIB_OPENSSL_SUPPORT")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/include")
    set(PLATFORM_LIBS ssl crypto pthread)
    set(PLATFORM_LIB_PATH /usr/lib)

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(PLATFORM "macOS_ARM")
    else()
        set(PLATFORM "macOS_x86")
    endif()
    
    set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
    set(OPENSSL_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    set(PLATFORM_LIB_PATH "${OPENSSL_ROOT_DIR}/lib")

    include_directories(${OPENSSL_INCLUDE_DIR})
    set(PLATFORM_LIBS ssl crypto pthread)
    link_directories(${PLATFORM_LIB_PATH})

elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM "Windows")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPPHTTPLIB_OPENSSL_SUPPORT")
    set(PLATFORM_LIBS libssl libcrypto ws2_32)
    set(PLATFORM_LIB_PATH "C:/path/to/openssl/lib")
    include_directories("C:/path/to/openssl/include")

else()
    message(FATAL_ERROR "Platform not supported")
endif()

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(manakan ${SOURCES})

target_link_libraries(manakan PRIVATE ${PLATFORM_LIBS})
link_directories(${PLATFORM_LIB_PATH})

set_target_properties(manakan PROPERTIES
    OUTPUT_NAME "manakan"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
)

