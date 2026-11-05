cmake_minimum_required(VERSION 3.20)
project(android_mirror LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

set(ENV{PKG_CONFIG_PATH} "$ENV{HOME}/.local/usr/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavutil libavcodec)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(client_core STATIC
    client/src/net/adb_tunnel.cpp
    client/src/net/socket_client.cpp
    client/src/video/video_demuxer.cpp
    client/src/net/control_serializer.cpp
    client/src/video/atomic_frame_queue.cpp
    client/src/video/video_decoder.cpp
    client/src/video/video_pipeline.cpp
    client/src/ui/video_renderer.cpp
    client/src/ui/input_mapper.cpp
    client/src/app/client_app.cpp
)
target_include_directories(client_core PUBLIC client/include)
target_link_libraries(client_core PUBLIC PkgConfig::FFMPEG PkgConfig::SDL2)

add_executable(mirror_client client/src/main.cpp)
target_link_libraries(mirror_client PRIVATE client_core)

add_executable(test_handshake_layout client/tests/test_handshake_layout.cpp)
target_include_directories(test_handshake_layout PRIVATE client/include)

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

enable_testing()
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
