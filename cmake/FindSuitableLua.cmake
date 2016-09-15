# We cant't use cmake's FindLuaXXX, since it doesn'tplay nicely
# when cross-compiling
set(luaversions "5.2;5.4;5.3;5.1")

macro(lua_detect_paths luaver)
  if (NOT CROSS_COMPILING)
    execute_process(COMMAND lua${luaver} ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua cpath ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
      OUTPUT_VARIABLE LUA_CPATH
      )
    execute_process(COMMAND lua${luaver} ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua path ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
        OUTPUT_VARIABLE LUA_LPATH
      )

  else()
    #Assume some sane defaults when cross-compiling
    SET(LUA_CPATH "lib/lua/${luaver}/" CACHE PATH "lua cpath")
    SET(LUA_LPATH "share/lua/${luaver}/" CACHE PATH "lua lpath")
    message("Cross-compiling: using predefined lua LPATH/LPATH")
  endif()

endmacro()

macro(lua_try_version version)
  PKG_CHECK_MODULES(LUA lua${version})
  if (LUA_FOUND)
    set(LUA_VERSION_FOUND ${version} CACHE STRING "lua version detected")
    lua_detect_paths(${LUA_VERSION_FOUND})
  endif()
endmacro()

foreach(v ${luaversions})
  lua_try_version(${v})
  if (LUA_FOUND)
    break()
  endif()
endforeach()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LUA
FOUND_VAR LUA_FOUND
REQUIRED_VARS LUA_INCLUDE_DIRS LUA_LIBRARIES LUA_LPATH LUA_CPATH)
