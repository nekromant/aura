# This file assumes that you have a linaro abe-based toolchain
# with a raspbian sysroot somewhere inside. This file also
# takes care to trick pkg-config into searching only toolchain's sysroot for
# the libraries

#Check if we're cross-compiling
if (NOT CROSS_COMPILE)
    return()
endif()

#Do not go further if there's a user-supplied cmake toolchain file
if (CMAKE_TOOLCHAIN_FILE)
    return()
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

#CMAKE_FIND_ROOT_PATH may have been defined by the top level
if (NOT CMAKE_FIND_ROOT_PATH)
    if (EXISTS ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/sysroot)
        SET(CMAKE_FIND_ROOT_PATH  ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/sysroot)
    elseif(EXISTS ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/libc)
        SET(CMAKE_FIND_ROOT_PATH  ${CROSS_TOOLCHAIN_PATH}/../${CROSS_COMPILE}/libc)
    else()
        message(WARNING "Couldn't auto-detect sysroot dir. Shit may happen")
    endif()
endif()

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
message(STATUS "Using cross-sysroot ${CMAKE_FIND_ROOT_PATH}")

#Rig our pkg-config helper only if we found root path
if (CMAKE_FIND_ROOT_PATH)
    include(PkgConfigSetup)
    PkgConfigSetupSysroot()
    PkgConfigAddDefaultPaths()
endif()
