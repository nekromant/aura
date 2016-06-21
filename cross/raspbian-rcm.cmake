# This file assumes that you have a linaro abe-based toolchain
# with a raspbian sysroot somewhere inside. This file also
# takes care to trick pkg-config into searching only sysroot for
# libraries

SET(CROSS_COMPILING yes)

SET(CROSS_TOOLCHAIN_PATH /home/necromant/x-tools/rcm-arm-linux-gnueabihf)
SET(CMAKE_LIBRARY_PATH arm-linux-gnueabihf)

# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH  ${CROSS_TOOLCHAIN_PATH}/${CMAKE_LIBRARY_PATH}/libc)

# specify the cross compiler
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=${CMAKE_FIND_ROOT_PATH}/lib/${CMAKE_LIBRARY_PATH}/")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath=${CMAKE_FIND_ROOT_PATH}/usr/lib/${CMAKE_LIBRARY_PATH}/")

SET(CMAKE_C_COMPILER     ${CROSS_TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER   ${CROSS_TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-g++)
# C++ test is broken withgout -Wl,-rpath
set(CMAKE_CXX_COMPILER_WORKS TRUE)

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
