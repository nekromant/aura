CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8)

PROJECT(aura)
SET(PROJECT_VERSION   0.1.2)
SET(AURA_API_VERSION  1)

enable_testing()
include(./utils.cmake)
#TODO: Move out linux-specific stuff

SET(SRCS
  buffer.c
  slog.c panic.c utils.c
  transport.c eventloop.c aura.c export.c serdes.c
  retparse.c queue.c utils-linux.c
  eventsys-epoll.c
  transport-dummy.c
  transport-sysfs-gpio.c
  packetizer.c
  crc.c
#  transport-serial.c
)
SET(TRANSPORTS dummy sysfs-gpio serial)

FIND_PACKAGE(PkgConfig)
PKG_CHECK_MODULES(LIBUSB libusb-1.0)
PKG_CHECK_MODULES(LUA52 lua5.2)
PKG_CHECK_MODULES(LUA51 lua5.1)
PKG_CHECK_MODULES(EASYNMC easynmc-0.1.1)

nc_build_library(aura 1 0.1.2 "${SRCS}")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -D_GNU_SOURCE")
#Version handling
EXECUTE_PROCESS(COMMAND git rev-parse HEAD
    OUTPUT_VARIABLE PROJECT_VERSION_GIT
    )
STRING(REGEX REPLACE "\n" "" PROJECT_VERSION_GIT ${PROJECT_VERSION_GIT})

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_VERSION='\"${PROJECT_VERSION}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_VERSION_GIT='\"${PROJECT_VERSION_GIT}\"'")

#Enable by default
SET(AURA_USE_BUFFER_POOL yes)
if(AURA_USE_BUFFER_POOL)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_USE_BUFFER_POOL")
endif()

# Let's auto detect a suitable lua version
if(LUA52_FOUND)
  SET(LUA_INCLUDE_DIRS ${LUA52_INCLUDE_DIRS})
  SET(LUA_LIBRARIES ${LUA52_LIBRARIES})
  SET(LUA_FOUND y)
  EXECUTE_PROCESS(COMMAND lua5.2 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua cpath ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_CPATH
    )
  EXECUTE_PROCESS(COMMAND lua5.2 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua path ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_LPATH
    )
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} lua5.2")
elseif(${LUA51_FOUND})
  SET(LUA_INCLUDE_DIRS ${LUA51_INCLUDE_DIRS})
  SET(LUA_LIBRARIES ${LUA51_LIBRARIES})
  SET(LUA_FOUND y)
  EXECUTE_PROCESS(COMMAND lua5.1 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua cpath ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_CPATH
    )
  EXECUTE_PROCESS(COMMAND lua5.1 ${CMAKE_SOURCE_DIR}/lua-guess-lib-install-path.lua path ${CMAKE_INSTALL_PREFIX} ${CMAKE_LIBRARY_PATH}
    OUTPUT_VARIABLE LUA_LPATH
    )
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} lua5.1")
endif()

if(LIBUSB_FOUND)
  SET(TRANSPORTS ${TRANSPORTS} usb)
  SET(SRCS ${SRCS} transport-usb.c usb-helpers.c)
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} libusb-1.0")
endif()

if(LIBUSB_FOUND AND LUA_FOUND)
    SET(TRANSPORTS ${TRANSPORTS} susb)
    SET(SRCS ${SRCS} transport-susb.c)
endif()

IF(LUA_FOUND)
  SET(SRCS ${SRCS} bindings-lua.c utils-lua.c)
  SET(BINDINGS ${BINDINGS} lua)
ENDIF()

IF(EASYNMC_FOUND)
  SET(TRANSPORTS ${TRANSPORTS} nmc)
  SET(SRCS ${SRCS} transport-nmc.c ion.c)
  SET(PKG_CONFIG_REQUIRES "${PKG_CONFIG_REQUIRES} easynmc-0.1.1")
ENDIF()

INCLUDE_DIRECTORIES(
    ./include/
    ${LIBUSB_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIRS}
    ${EASYNMC_INCLUDE_DIRS}
)

ADD_LIBRARY(aura OBJECT ${SRCS})

ADD_LIBRARY(aurastatic STATIC $<TARGET_OBJECTS:aura>)
SET_TARGET_PROPERTIES(aurastatic PROPERTIES OUTPUT_NAME aura)
TARGET_LINK_LIBRARIES(aurastatic
    ${LIBUSB_LIBRARIES}
    ${LUA_LIBRARIES}
    ${EASYNMC_LIBRARIES}
)

ADD_LIBRARY(aurashared SHARED $<TARGET_OBJECTS:aura>)
SET_TARGET_PROPERTIES(aurashared PROPERTIES OUTPUT_NAME aura)
TARGET_LINK_LIBRARIES(aurashared
    ${LIBUSB_LIBRARIES}
    ${LUA_LIBRARIES}
    ${EASYNMC_LIBRARIES}
    -lrt -lm
)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
file(WRITE ${CMAKE_SOURCE_DIR}/.clang_complete "")
foreach(dir ${dirs})
  file(APPEND ${CMAKE_SOURCE_DIR}/.clang_complete "-I${dir}\n")
endforeach()
message(STATUS "Generarating ${CMAKE_SOURCE_DIR}/.clang_complete")

# UNIT TESTS OF THE CORE API
file(GLOB UNITS
    "${CMAKE_SOURCE_DIR}/tests/*.c"
)

foreach(file ${UNITS})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)
  ADD_EXECUTABLE(${f} ${file})
  TARGET_LINK_LIBRARIES(${f} aurashared)
  ADD_TEST(${f} ${f})
  if (TEST_LEAKS)
    ADD_TEST(memcheck-${f} valgrind
      --error-exitcode=1 --read-var-info=yes
      --leak-check=full  --show-leak-kinds=all
      --undef-value-errors=no --xml=yes --xml-file=${f}.xml
      ./${f})
  endif()
endforeach(file)

# BENCHMARKS OF THE CORE API (We won't run them during CI)
file(GLOB BENCHES
    "${CMAKE_SOURCE_DIR}/benchmarks/*.c"
)

foreach(file ${BENCHES})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)
  ADD_EXECUTABLE(bench-${f} ${file})
  TARGET_LINK_LIBRARIES(bench-${f} aurashared)
endforeach(file)

# Transport plugins tests (need hardware to run)
file(GLOB TTESTS
    "${CMAKE_SOURCE_DIR}/tests-transports/*.c"
)

foreach(file ${TTESTS})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)
  ADD_EXECUTABLE(${f} ${file})
  TARGET_LINK_LIBRARIES(${f} aurashared)
  if (TEST_TRANSPORTS)
    ADD_TEST(${f} ${f})
    if (TEST_LEAKS)
      ADD_TEST(memcheck-${f} valgrind
	--error-exitcode=1 --read-var-info=yes
	--leak-check=full  --show-leak-kinds=all
	--undef-value-errors=no --xml=yes --xml-file=${f}.xml
	./${f})
    endif()
  endif()
endforeach(file)


SET_TARGET_PROPERTIES(aurashared PROPERTIES SOVERSION ${PROJECT_VERSION}
  VERSION ${AURA_API_VERSION})

message("libaura version:           ${PROJECT_VERSION}")
message("libaura git commit:        ${PROJECT_VERSION_GIT}")
message("Enabled transports:        ${TRANSPORTS}")
message("Enabled language bindings: ${BINDINGS}")
message("Install prefix:            ${CMAKE_INSTALL_PREFIX}")

if (AURA_USE_BUFFER_POOL)
message("Buffer pooling:            enabled")
else()
message("Buffer pooling:            disabled")
endif()

if(LUA_FOUND)
  message("lua .so dir:               ${CMAKE_INSTALL_PREFIX}/${LUA_CPATH}")
  message("lua lib dir:               ${CMAKE_INSTALL_PREFIX}/${LUA_LPATH}")
endif()

INSTALL(TARGETS aurashared LIBRARY
        DESTINATION lib/${CMAKE_LIBRARY_PATH})

INSTALL(TARGETS aurastatic ARCHIVE
        DESTINATION lib/${CMAKE_LIBRARY_PATH})

file(GLOB LUA_SCRIPTS
    "${CMAKE_SOURCE_DIR}/lua/aura/*"
)

# If we have lua - let's install all the lua stuff
if (LUA_FOUND)
INSTALL(FILES ${LUA_SCRIPTS}
       DESTINATION ${LUA_LPATH}/aura)

INSTALL(FILES lua/aura.lua
       DESTINATION ${LUA_LPATH})

#FixMe: Hack. CMake symlink handling is shitty and obscure
add_custom_command(TARGET aurashared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:aurashared> auracore.so
)

INSTALL(FILES ${CMAKE_BINARY_DIR}/auracore.so DESTINATION ${LUA_CPATH})
endif()

# Now, let's install headers
file(GLOB AURA_HEADERS
    "${CMAKE_SOURCE_DIR}/include/aura/*"
)

file(GLOB ION_HEADERS
    "${CMAKE_SOURCE_DIR}/include/ion/*"
)

INSTALL(FILES ${AURA_HEADERS}
       DESTINATION include/${CMAKE_LIBRARY_PATH}/aura-${PROJECT_VERSION}/aura)

INSTALL(FILES ${ION_HEADERS}
       DESTINATION include/${CMAKE_LIBRARY_PATH}/ion)

#Finally, the pkg-config file
SET(PKG_CONFIG_LIBDIR
    "\${prefix}/lib/\${deb_host_multiarch}"
)
SET(PKG_CONFIG_INCLUDEDIR
    "\${prefix}/include/\${deb_host_multiarch}/aura-${PROJECT_VERSION}"
)
SET(PKG_CONFIG_LIBS
    "-L\${libdir} -l:libaura.so.${PROJECT_VERSION}"
)
SET(PKG_CONFIG_CFLAGS
    "-I\${includedir}"
)

CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
       DESTINATION lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
