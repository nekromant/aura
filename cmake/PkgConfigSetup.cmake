#Add a new path to search for .pc files
macro(PkgConfigAddPath NEWPATH)
    if (NOT _PKG_CONFIG_INIT_ENV)
        message(FATAL_ERROR "Please do PkgConfigSetupSysroot() before adding paths")
    endif()
    if (WIN32)
        SET(SEPARATOR ";")
    else()
        SET(SEPARATOR ":")
    endif()
    SET(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}${SEPARATOR}${NEWPATH}")
endmacro()

macro (PkgConfigForceSysroot sysroot libdir)
    if (UNIX)
        SET(ENV{PKG_CONFIG_SYSROOT_DIR} ${sysroot})
    endif()
    SET(ENV{PKG_CONFIG_LIBDIR} ${sysroot}${libdir})
message($ENV{PKG_CONFIG_LIBDIR})
message($ENV{PKG_CONFIG_LIBDIR})
endmacro()

#Tell pkg-config where to look for libraries.
#SYSROOT dir only makes sense and works on linux
macro (PkgConfigSetupSysroot)
    SET(_PKG_CONFIG_INIT_ENV true)
    if (NOT CMAKE_FIND_ROOT_PATH)
        message(FATAL_ERROR "Please set CMAKE_FIND_ROOT_PATH before calling PkgConfigSetupSysroot")
    endif()
    PkgConfigForceSysroot(${CMAKE_FIND_ROOT_PATH} "/usr/lib/pkgconfig/")
endmacro()

# Add some sane default path suitable for debian-multiarch
# This one needs CMAKE_LIBRARY_PATH defined
macro(PkgConfigAddDefaultPaths)
    if (NOT CMAKE_LIBRARY_PATH)
        message(FATAL_ERROR "Please define CMAKE_LIBRARY_PATH before calling PkgConfigAddDefaultPaths()")
    endif()
    PkgConfigAddPath(${CMAKE_FIND_ROOT_PATH}/usr/lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
    PkgConfigAddPath("${CMAKE_FIND_ROOT_PATH}/usr/share/pkgconfig/")
endmacro()
