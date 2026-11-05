cmake_minimum_required(VERSION 3.20)
project(MirrorDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Retain assertions in test suites even during Release builds
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wconversion)
endif()

include(GNUInstallDirs)

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
    client/src/session/android_adb_session.cpp
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

# Primary MirrorDesktop executable (canonical output name: android-mirror)
add_executable(MirrorDesktop client/src/main.cpp)
set_target_properties(MirrorDesktop PROPERTIES OUTPUT_NAME "android-mirror")
target_include_directories(MirrorDesktop PRIVATE client/include include ${SDL2_INCLUDE_DIRS})
target_link_libraries(MirrorDesktop PRIVATE client_core)

if(WIN32)
    target_sources(MirrorDesktop PRIVATE res/manifest.rc)
endif()

# Provide mirror-desktop target
add_custom_target(mirror-desktop ALL DEPENDS MirrorDesktop)

# Alias for backwards compatibility
add_custom_target(mirror_client ALL DEPENDS MirrorDesktop)

# Alias desktop-client and MirrorDesktop binaries for parity
add_custom_command(TARGET MirrorDesktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:MirrorDesktop> ${CMAKE_CURRENT_BINARY_DIR}/MirrorDesktop
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:MirrorDesktop> ${CMAKE_CURRENT_BINARY_DIR}/desktop-client
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:MirrorDesktop> ${CMAKE_CURRENT_BINARY_DIR}/mirror_client
    COMMENT "Generating MirrorDesktop, desktop-client, and mirror_client symlinks"
)

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

# --- Installation Rules ---
install(TARGETS MirrorDesktop
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/android-mirror.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    OPTIONAL
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/build/server.jar")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/build/server.jar"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/android-mirror"
        RENAME server.jar
    )
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/android-server/build/libs/server.jar")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/android-server/build/libs/server.jar"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/android-mirror"
        RENAME server.jar
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/android-mirror.png")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/android-mirror.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME android-mirror.png
    )
endif()

# --- CPack Debian Packaging Configuration ---
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "android-mirror")
set(CPACK_PACKAGE_VENDOR "Android Mirror")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Low-latency Android screen mirroring and input control client")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_CONTACT "Megh Nair <megh@localhost>")
set(CPACK_PACKAGE_MAINTAINER "Megh Nair <megh@localhost>")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Megh Nair <megh@localhost>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libavcodec62 | libavcodec60 | libavcodec58, libavutil60 | libavutil58, adb")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Microsoftened-Nair/android-mirror")

include(CPack)
