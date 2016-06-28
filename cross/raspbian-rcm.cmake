# This file assumes that you have a linaro abe-based toolchain
# with a raspbian sysroot somewhere inside. This file also
# takes care to trick pkg-config into searching only toolchain's sysroot for
# the libraries

SET(CROSS_COMPILING yes)

SET(CROSS_TRIPLET arm-rcm-linux-gnueabihf)
SET(CMAKE_LIBRARY_PATH arm-linux-gnueabihf)

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_C_COMPILER     ${CROSS_TRIPLET}-gcc${CMAKE_EXECUTABLE_SUFFIX})
SET(CMAKE_CXX_COMPILER   ${CROSS_TRIPLET}-g++${CMAKE_EXECUTABLE_SUFFIX})

#get_filename_component(CROSS_TOOLCHAIN_PATH "${CMAKE_C_COMPILER}" ABSOLUTE)
find_program(CROSS_TOOLCHAIN_PATH NAMES ${CMAKE_C_COMPILER})
get_filename_component(CROSS_TOOLCHAIN_PATH "${CROSS_TOOLCHAIN_PATH}" PATH)
message(${CROSS_TOOLCHAIN_PATH})

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH  ${CROSS_TOOLCHAIN_PATH}/../${CROSS_TRIPLET}/libc)


# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

#Since autodetection will not work as expected when cross-compiling
#Let's fill in details
SET(LUA_CPATH "lib/lua/5.2/")
SET(LUA_LPATH "share/lua/5.2/")
SET(CMAKE_INSTALL_PREFIX "/usr")

SET(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_FIND_ROOT_PATH})
SET(ENV{PKG_CONFIG_LIBDIR} ${CMAKE_FIND_ROOT_PATH}/usr/lib/pkgconfig/)

SET(ENV{PKG_CONFIG_PATH} ${CMAKE_FIND_ROOT_PATH}/usr/lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${CMAKE_FIND_ROOT_PATH}/usr/share/pkgconfig/")
