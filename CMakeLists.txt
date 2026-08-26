cmake_minimum_required(VERSION 3.16)

project(gaelco-loader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

#
# SDL2
#

find_program(SDL2_CONFIG sdl2-config REQUIRED)

execute_process(
    COMMAND ${SDL2_CONFIG} --cflags
    OUTPUT_VARIABLE SDL2_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${SDL2_CONFIG} --libs
    OUTPUT_VARIABLE SDL2_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(SDL2_CFLAGS_LIST UNIX_COMMAND "${SDL2_CFLAGS}")
separate_arguments(SDL2_LIBS_LIST UNIX_COMMAND "${SDL2_LIBS}")

#
# gaelco-preload.so
#


find_path(XF86VM_INCLUDE_DIR
    X11/extensions/xf86vmode.h
)

if(NOT XF86VM_INCLUDE_DIR)
    message(FATAL_ERROR "XFree86-VidMode headers not found")
endif()

add_library(gaelco-preload SHARED
    src/preload.c
    src/controls.c
    src/gaelco.c
    src/graphics.c
    src/sound.c
    src/filesystem.c
)

target_include_directories(gaelco-preload PRIVATE
    ${XF86VM_INCLUDE_DIR}
)

target_compile_options(gaelco-preload PRIVATE
    -m32
    -fPIC
    -fno-stack-protector
    ${SDL2_CFLAGS_LIST}
)

target_link_options(gaelco-preload PRIVATE
    -m32
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/preload.map
)

target_link_libraries(gaelco-preload PRIVATE
    ${SDL2_LIBS_LIST}
    X11
    Xxf86vm
    dl
)

set_target_properties(gaelco-preload PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gaelco-preload"
)

#
# gaelco launcher
#

add_executable(gaelco
    src/gaelco.c
)

target_compile_options(gaelco PRIVATE
    -m32
)

target_link_options(gaelco PRIVATE
    -m32
)