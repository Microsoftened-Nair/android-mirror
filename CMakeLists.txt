cmake_minimum_required(VERSION 3.20)
project(android_mirror LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(client_core STATIC
    client/src/net/adb_tunnel.cpp
    client/src/net/socket_client.cpp
)
target_include_directories(client_core PUBLIC client/include)

add_executable(test_handshake_layout client/tests/test_handshake_layout.cpp)
target_include_directories(test_handshake_layout PRIVATE client/include)

add_executable(test_adb_tunnel client/tests/test_adb_tunnel.cpp)
target_link_libraries(test_adb_tunnel PRIVATE client_core)

add_executable(test_socket_handshake client/tests/test_socket_handshake.cpp)
target_link_libraries(test_socket_handshake PRIVATE client_core)
