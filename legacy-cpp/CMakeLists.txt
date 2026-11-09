cmake_minimum_required(VERSION 3.20)
project(halcyon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(halcyon
    src/main.cpp
    src/common.cpp
    src/mem.cpp
    src/config.cpp
    src/game.cpp
    src/overlay.cpp
    src/aim.cpp
    src/esp.cpp
    src/menu.cpp
    src/misc.cpp
)

target_compile_definitions(halcyon PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(halcyon PRIVATE
    user32
    gdi32
    gdiplus
    winmm
)

if(MSVC)
    target_compile_options(halcyon PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(halcyon PRIVATE -Wall -Wextra -O2)
    target_link_options(halcyon PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(halcyon PROPERTIES OUTPUT_NAME "halcyon")

add_custom_command(TARGET halcyon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../config.ini"
            "$<TARGET_FILE_DIR:halcyon>/config.ini"
)
