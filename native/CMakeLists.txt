cmake_minimum_required(VERSION 3.10)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(hello)
set(CMAKE_CXX_STANDARD 17)

set(DAWN_ENABLE_PIC        ON CACHE BOOL "Position-Independent-Code")
set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "OpenGL backend")
set(DAWN_ENABLE_OPENGLES   OFF CACHE BOOL "OpenGL ES backend")
set(DAWN_BUILD_EXAMPLES    OFF CACHE BOOL "Dawn examples")
set(TINT_BUILD_SAMPLES     OFF CACHE BOOL "Tint examples")
set(TINT_BUILD_GLSL_WRITER OFF CACHE BOOL "OpenGL SL writer")

if(NOT EMSCRIPTEN)
    add_subdirectory("third_party/dawn" EXCLUDE_FROM_ALL)
endif()

if(EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
endif()

add_executable(hello
    "main.cpp"
    )

if(EMSCRIPTEN)
    set_target_properties(hello PROPERTIES
        SUFFIX ".html")
    target_link_options(hello
        PRIVATE "SHELL:-gsource-map"
        PRIVATE "SHELL:--source-map-base https://kai.graphics/webgpu-cross-platform-demo/out/web/"
        PRIVATE "SHELL:-s USE_WEBGPU=1"
        PRIVATE "SHELL:-s ASSERTIONS=1"
        PRIVATE "SHELL:-s SAFE_HEAP=1"
        #PRIVATE "SHELL:--closure 1"
        #PRIVATE "SHELL:-s VERBOSE=1"
        )
else()
    target_link_libraries(hello
        dawn_headers
        dawncpp
        dawn_native
        dawn_proc
        )
endif()
