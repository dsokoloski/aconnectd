project(aconnectd)
cmake_minimum_required(VERSION 3.2)

set(PACKAGE_VERSION "1.0.0")
add_definitions(-DPACKAGE_VERSION=\"${PACKAGE_VERSION}\")

add_definitions("-std=gnu++11 -Wall -Werror")

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fstack-protector-strong")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer")

include(FindPkgConfig)
pkg_check_modules(ALSA REQUIRED alsa)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(deploy)
