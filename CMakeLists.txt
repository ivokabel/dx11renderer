cmake_minimum_required(VERSION 3.5)

if (NOT WIN32)
    message(FATAL_ERROR "Only Windows environment is supported for this project!")
endif()

project(dx11renderer LANGUAGES CXX)

set(RENDERER_SOURCES
    WIN32
    constants.hpp
    main.cpp
    irenderingcontext.hpp
    renderer.hpp
    renderer.cpp
    post_shaders.fx
    iscene.hpp
    scene.hpp
    scene.cpp
    scene_shaders.fx
    scene_utils.hpp
    scene_utils.cpp
    gltf_utils.hpp
    gltf_utils.cpp
    log.hpp
    log.cpp
    utils.hpp
    utils.cpp
    Libs/tinygltf-2.2.0/stb_image.h
    Libs/tinygltf-2.2.0/stb_image_write.h
    Libs/tinygltf-2.2.0/tiny_gltf.h
    Libs/tinygltf-2.2.0/json.hpp

    #debug
    #Libs/tinygltf-2.2.0/loader_example.h
    #Libs/tinygltf-2.2.0/loader_example.cc
    )

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
    add_compile_options(/W4)
    add_definitions(-DUNICODE -D_UNICODE)
endif()
#if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU" )
#    add_compile_options(-Wall -Wextra -Wpedantic -Wunreachable-code)
#endif()

# A little workaround 0:-)
# Needs to be called before creating a target
link_directories("$ENV{DXSDK_DIR}Lib\\x64\\")

add_executable(dx11renderer ${RENDERER_SOURCES})
#install(TARGETS dx11renderer DESTINATION bin)

# DirectX
include_directories("$ENV{DXSDK_DIR}Include\\")
target_link_libraries(dx11renderer
    d3d11
    d3dcompiler
    d3dx11d
    d3dx9d
    dxerr
    dxguid
    dxgi
    winmm
    comctl32
    )

if (${CMAKE_VERSION} VERSION_GREATER 3.5)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT dx11renderer)
endif()
