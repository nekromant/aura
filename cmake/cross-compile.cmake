# This file assumes that you have a linaro abe-based toolchain
# with a raspbian sysroot somewhere inside. This file also
# takes care to trick pkg-config into searching only toolchain's sysroot for
# the libraries

SET(CROSS_COMPILING yes)

if (NOT CROSS_COMPILE)
    message(FATAL_ERROR "We're not cross-compiling: ${CROSS_COMPILE}")
endif()

macro(SET_DEFAULT_VALUE _VAR _VALUE)
    if (NOT ${_VAR})
        SET(${_VAR} "${_VALUE}")
    endif()
endmacro()

SET_DEFAULT_VALUE(CMAKE_LIBRARY_PATH "${CROSS_COMPILE}")

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

SET_DEFAULT_VALUE(CMAKE_C_COMPILER     ${CROSS_COMPILE}-gcc${CMAKE_EXECUTABLE_SUFFIX})
SET_DEFAULT_VALUE(CMAKE_CXX_COMPILER   ${CROSS_COMPILE}-g++${CMAKE_EXECUTABLE_SUFFIX})

find_program(CROSS_TOOLCHAIN_PATH NAMES ${CMAKE_C_COMPILER})
get_filename_component(CROSS_TOOLCHAIN_PATH "${CROSS_TOOLCHAIN_PATH}" PATH)

if (EXISTS ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/sysroot)
    SET(CMAKE_FIND_ROOT_PATH  ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/sysroot)
elseif(EXISTS ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/libc)
    SET(CMAKE_FIND_ROOT_PATH  ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/libc)
else()
    message(WARNING "Couldn't auto-detect sysroot dir")
endif()

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
message(STATUS "Using cross-sysroot ${CMAKE_FIND_ROOT_PATH}")

#Since autodetection will not work as expected when cross-compiling
#Let's fill in details
SET_DEFAULT_VALUE(LUA_CPATH "lib/lua/5.2/")
SET_DEFAULT_VALUE(LUA_LPATH "share/lua/5.2/")

#Tell pkg-config where to look for libraries
SET(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_FIND_ROOT_PATH})
SET(ENV{PKG_CONFIG_LIBDIR} ${CMAKE_FIND_ROOT_PATH}/usr/lib/pkgconfig/)

SET(ENV{PKG_CONFIG_PATH} ${CMAKE_FIND_ROOT_PATH}/usr/lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_FIND_ROOT_PATH}/usr/share/pkgconfig/")
