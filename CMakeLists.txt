cmake_minimum_required(VERSION 3.20)
project(MirrorDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Setup paths for custom/local tool locations
list(PREPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local/usr/lib/x86_64-linux-gnu/cmake/SDL2" "$ENV{HOME}/.local/usr")
set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/.local/usr/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET libavcodec libavutil libavformat)

set(CLIENT_SOURCES
    client/src/net/adb_tunnel.cpp
    client/src/net/socket_client.cpp
    client/src/net/control_serializer.cpp
    client/src/video/video_demuxer.cpp
    client/src/video/atomic_frame_queue.cpp
    client/src/video/video_decoder.cpp
    client/src/video/video_pipeline.cpp
    client/src/ui/video_renderer.cpp
    client/src/ui/input_mapper.cpp
    client/src/app/client_app.cpp
)

add_library(client_core STATIC ${CLIENT_SOURCES})
target_include_directories(client_core PUBLIC client/include include ${SDL2_INCLUDE_DIRS})
target_link_libraries(client_core PUBLIC SDL2::SDL2 PkgConfig::LIBAV)

if(WIN32)
    target_link_libraries(client_core PUBLIC ws2_32)
elseif(APPLE)
    target_link_libraries(client_core PUBLIC "-framework Cocoa" "-framework Metal" "-framework CoreVideo")
elseif(UNIX)
    target_link_libraries(client_core PUBLIC pthread)
endif()

# Primary MirrorDesktop executable
add_executable(MirrorDesktop client/src/main.cpp)
target_include_directories(MirrorDesktop PRIVATE client/include include ${SDL2_INCLUDE_DIRS})
target_link_libraries(MirrorDesktop PRIVATE client_core)

if(WIN32)
    target_sources(MirrorDesktop PRIVATE res/manifest.rc)
endif()

# Provide mirror-desktop target
add_custom_target(mirror-desktop ALL DEPENDS MirrorDesktop)

# Alias for backwards compatibility
add_custom_target(mirror_client ALL DEPENDS MirrorDesktop)

# Unit Test Targets & CTest
enable_testing()

add_executable(test_handshake_layout client/tests/test_handshake_layout.cpp)
target_include_directories(test_handshake_layout PRIVATE client/include include)

add_executable(test_adb_tunnel client/tests/test_adb_tunnel.cpp)
target_link_libraries(test_adb_tunnel PRIVATE client_core)

add_executable(test_socket_handshake client/tests/test_socket_handshake.cpp)
target_link_libraries(test_socket_handshake PRIVATE client_core)

add_executable(test_video_demuxer client/tests/test_video_demuxer.cpp)
target_link_libraries(test_video_demuxer PRIVATE client_core)

add_executable(test_control_packet client/tests/test_control_packet.cpp)
target_link_libraries(test_control_packet PRIVATE client_core)

add_executable(test_atomic_frame_queue client/tests/test_atomic_frame_queue.cpp)
target_link_libraries(test_atomic_frame_queue PRIVATE client_core)

add_executable(test_video_decoder client/tests/test_video_decoder.cpp)
target_link_libraries(test_video_decoder PRIVATE client_core)

add_executable(test_video_pipeline client/tests/test_video_pipeline.cpp)
target_link_libraries(test_video_pipeline PRIVATE client_core)

add_executable(test_video_renderer client/tests/test_video_renderer.cpp)
target_link_libraries(test_video_renderer PRIVATE client_core)

add_executable(test_input_mapper client/tests/test_input_mapper.cpp)
target_link_libraries(test_input_mapper PRIVATE client_core)

add_executable(test_client_app client/tests/test_client_app.cpp)
target_link_libraries(test_client_app PRIVATE client_core)

add_test(NAME handshake_layout COMMAND test_handshake_layout)
add_test(NAME adb_tunnel COMMAND test_adb_tunnel)
add_test(NAME socket_handshake COMMAND test_socket_handshake)
add_test(NAME video_demuxer COMMAND test_video_demuxer)
add_test(NAME control_packet COMMAND test_control_packet)
add_test(NAME atomic_frame_queue COMMAND test_atomic_frame_queue)
add_test(NAME video_decoder COMMAND test_video_decoder)
add_test(NAME video_pipeline COMMAND test_video_pipeline)
add_test(NAME video_renderer COMMAND test_video_renderer)
add_test(NAME input_mapper COMMAND test_input_mapper)
add_test(NAME client_app COMMAND test_client_app)
