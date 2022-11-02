cmake_minimum_required(VERSION 3.10)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(hello)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-pthread)

if(NOT EMSCRIPTEN)
    # sudo apt-get install libglfw3-dev
    find_package(glfw3 REQUIRED)

    # set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    # set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    # set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    # add_subdirectory("third_party/glfw3")

    set(DAWN_ENABLE_PIC        ON CACHE BOOL "Position-Independent-Code")
    set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "OpenGL backend")
    set(DAWN_ENABLE_OPENGLES   OFF CACHE BOOL "OpenGL ES backend")
    set(DAWN_BUILD_EXAMPLES    OFF CACHE BOOL "Dawn examples")
    set(TINT_BUILD_SAMPLES     OFF CACHE BOOL "Tint examples")
    set(TINT_BUILD_GLSL_WRITER OFF CACHE BOOL "OpenGL SL writer")

    set(DAWN_USE_X11 ON CACHE BOOL "Dawn use X11")
    set(DAWN_GLFW_DIR "third_party/glfw3" CACHE BOOL "Dawn use GLFW")

    add_subdirectory("third_party/dawn" EXCLUDE_FROM_ALL)


    # # add_subdirectory("src-native")
    # file(GLOB NATIVE_SRC_FILES    
    #     "src-native/*.h"
    #     "src-native/*.cpp"
    # )

    # add_executable(hello
    #     ${NATIVE_SRC_FILES}
    #     "main.cpp"
    #     )


    set(SOURCES
        "vec3.h"
        "mat4.h"
        "mat4.cc"

        "input.h"
        "window.h"
        # "window.cpp"

        # # temp try
        # "out/native/third_party/dawn/gen/src/dawn/webgpu_cpp.cpp"
    )

    add_executable(hello
        ${SOURCES}
        "main.cpp"
        )

    # target_include_directories(hello
    #     PRIVATE
    #     ${CMAKE_CURRENT_BINARY_DIR}/third_party/dawn
    # )

    target_include_directories(hello
        PRIVATE
        "third_party/glfw3/include"
        "out/native/third_party/dawn/gen/src/dawn/include"
    )

    # target_sources(hello
    #     PRIVATE
    #     "input.h"
    #     "window.h"
    #     "window.cpp"
    # )
endif()

if(EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

    add_executable(hello
        "vec3.h"
        "mat4.h"
        "mat4.cc"
        "main.cpp"
        )
endif()

# add_executable(hello
#     "main.cpp"
#     )

if(EMSCRIPTEN)
    set_target_properties(hello PROPERTIES
        SUFFIX ".html")
    target_link_options(hello
        PRIVATE "SHELL:-gsource-map"
        PRIVATE "SHELL:--source-map-base ./"
        PRIVATE "SHELL:-s USE_WEBGPU=1"
        PRIVATE "SHELL:-s ASSERTIONS=1"
        PRIVATE "SHELL:-s SAFE_HEAP=1"
        #PRIVATE "SHELL:--closure 1"
        #PRIVATE "SHELL:-s VERBOSE=1"

        PRIVATE "SHELL:-s USE_PTHREADS=1"
        PRIVATE "SHELL:-s PTHREAD_POOL_SIZE=4"
        # PRIVATE "SHELL:-s PROXY_TO_PTHREAD"
        )
else()
    target_link_libraries(hello
        # dawn_public_config
        dawn_headers
        dawncpp_headers
        dawncpp
        dawn_native
        dawn_proc
        # librt
        webgpu_glfw
        glfw
        )
endif()
