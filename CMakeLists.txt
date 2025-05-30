cmake_minimum_required(VERSION 3.10)
project(ffpp LANGUAGES CXX)

add_compile_options(-O3 -ffast-math)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavcodec
  libavformat
  libavutil
  libswscale
)

# Sources
set(SOURCES
  ffpp.cpp
  api.cpp
)

# FFPP .so
add_library(ffpp SHARED ${SOURCES})
target_include_directories(ffpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ffpp
  PkgConfig::FFMPEG
  stdc++fs
)

# Install .hpp and .so
install(FILES ffpp.hpp DESTINATION include/ffpp)
install(TARGETS ffpp LIBRARY DESTINATION lib)

# ldconfig run after install
install(CODE "execute_process(COMMAND ldconfig)")

# uninstall
add_custom_target("uninstall" COMMENT "Uninstall installed files")
add_custom_command(
    TARGET "uninstall"
    POST_BUILD
    COMMENT "Uninstall files with install_manifest.txt"
    COMMAND xargs rm -vf < install_manifest.txt || echo Nothing in
            install_manifest.txt to be uninstalled!
)

