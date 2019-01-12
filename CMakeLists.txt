cmake_minimum_required(VERSION 3.5)

if (NOT WIN32)
    message(FATAL_ERROR "Only Windows environment is supported for this project!")
endif()

project(dx11renderer LANGUAGES CXX)

set(RENDERER_SOURCES
    WIN32
    main.cpp
    renderer.hpp
    renderer.cpp
    )

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(dx11renderer ${RENDERER_SOURCES})
#install(TARGETS dx11renderer DESTINATION bin)

if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    add_compile_options(/W4)
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU" )
    add_compile_options(-Wall -Wextra -Wpedantic -Wunreachable-code)
endif()

if (${CMAKE_VERSION} VERSION_GREATER 3.5)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT dx11renderer)
endif()
