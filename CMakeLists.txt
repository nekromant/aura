CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

include(LinuxCrossCompile)
find_package(PkgConfig)
include(AuraMacros)
include(AuraTesting)
project(aura)

#TODO: Set this from git tag
set(PROJECT_VERSION   0.1.3)
set(PROJECT_DOC_VERSION   gitmaster)
set(AURA_API_VERSION  3)


#Safe to enable, pool is stable
SET(AURA_USE_BUFFER_POOL yes CACHE BOOL "Enable buffer pool")


SET(AURA_BUILD_DOC no CACHE BOOL "Build doxygen & ldoc documentation")
#DEVELOPER HACKS. Enable only when hacking around aura
SET(AURA_TEST_TIMEOUT 30 CACHE STRING "Test timeout in seconds")
SET(AURA_TEST_LEAKS no CACHE BOOL "Valgrind for memory leaks during testing (Runs each test twice)")
SET(AURA_TEST_USB_TRANSPORT no CACHE BOOL "Run usb/susb transport module test suite (requires hardware)")
SET(AURA_TEST_STLINK_TRANSPORT no CACHE BOOL "Run stlink transport module test suite (requires hardware)")
SET(AURA_TEST_NMC_TRANSPORT no CACHE BOOL "Run Neuromatrix(r) DSP transport module test suite (requires hardware)")
SET(AURA_TEST_DUMMY yes CACHE BOOL "Run basic test suite on a dummy transport (no hw required)")
SET(AURA_TEST_BINDINGS yes CACHE BOOL "Run basic lua bindings test suite (no hw required)")
SET(AURA_COVERALLS no CACHE BOOL "Generate coveralls data")
SET(AURA_TEST_REMOTE no CACHE BOOL "Run testsuite on remote machine")
SET(AURA_TEST_REMOTE_HOST "root@shade.home" CACHE STRING "Run tests on a remote machine")
SET(AURA_TEST_PREFIX "host" CACHE STRING "Prefix for test name")
SET(AURA_EXTRA_VALGRIND_OPTS "--show-leak-kinds=all" CACHE STRING "Extra options for valgrind")

SET(CTEST_ENVIRONMENT
  "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua"
  "AURA_BUILDDIR=${CMAKE_BINARY_DIR}"
  "AURA_SOURCEDIR=${CMAKE_SOURCE_DIR}"
)


aura_add_source_in_dir(src/core
    buffer.c
    slog.c panic.c utils.c
    transport.c eventloop.c aura.c export.c serdes.c
    eventloop-factory.c timer.c
    retparse.c queue.c
    libevent-helpers.c
)

aura_add_source_in_dir(src/allocators/
    ion.c ion_buffer_allocator.c
)

# Our dependencies
# Lua needs some x-tra love
find_package(SuitableLua)

if(LUA_FOUND)
  aura_add_include_dirs(${LUA_INCLUDE_DIRS})
  aura_add_link_libraries(${LUA_LIBRARIES})
  aura_add_source_in_dir(src/lua bindings.c utils-lua.c)
  # We need our dedicated lua test wrapper for testing...
  add_executable(lua-test-wrapper src/lua/lua-test-wrapper.c src/lua/utils-lua.c)
  target_link_libraries(lua-test-wrapper ${LUA_LIBRARIES})
  ADD_SCRIPT_TEST_DIRECTORY(lua bindings lua ${AURA_TEST_BINDINGS} ${AURA_TEST_LEAKS})
endif()

# The rest are pretty straightforward
aura_pkg_require(LIBUSB libusb-1.0)
aura_pkg_require(EASYNMC easynmc-0.1.1)
aura_pkg_require(LIBEVENT libevent)
aura_pkg_require(STLINK stlink)

#Add our eventloop modules
aura_add_eventloop(epoll src/eventloop/epoll.c)

if(LIBEVENT_FOUND)
  list(APPEND CMAKE_C_FLAGS -DAURA_HAS_LIBEVENT)
  aura_add_eventloop(libevent src/eventloop/libevent.c)
endif()

#Add all our transport modules
aura_add_transport(null       src/transports/transport-null.c)
aura_add_transport(dummy      src/transports/transport-dummy.c)
aura_add_transport(sysfs-gpio src/transports/transport-sysfs-gpio.c)
ADD_C_TEST_DIRECTORY(dummy dummy ${AURA_TEST_DUMMY} ${AURA_TEST_LEAKS})

#aura_add_transport(serial transport-serial.c)

if (STLINK_FOUND)
  aura_add_transport(stlink src/transports/transport-stlink.c)
  ADD_C_TEST_DIRECTORY(stlink stlink ${AURA_TEST_STLINK_TRANSPORT} ${AURA_TEST_LEAKS})
endif()

if(LIBUSB_FOUND)
  aura_add_transport(usb src/transports/transport-usb.c src/transports/usb-helpers.c)
endif()

if(LIBUSB_FOUND AND LUA_FOUND)
  aura_add_transport(susb src/transports/transport-susb.c)
  ADD_C_TEST_DIRECTORY(xUSB xusb ${AURA_TEST_USB_TRANSPORT} ${AURA_TEST_LEAKS})
endif()

if(EASYNMC_FOUND)
  aura_add_transport(nmc src/transports/transport-nmc.c)
  ADD_C_TEST_DIRECTORY(nmc nmc ${AURA_TEST_NMC_TRANSPORT} ${AURA_TEST_LEAKS})
  file(COPY ${CMAKE_SOURCE_DIR}/nmc-dsp-test-app.abs DESTINATION ${CMAKE_BINARY_DIR})
  file(RENAME ${CMAKE_BINARY_DIR}/nmc-dsp-test-app.abs ${CMAKE_BINARY_DIR}/aura-test.abs)
endif()

find_package(Doxygen)
find_program(LDOC_FOUND ldoc)

if ( ${CMAKE_BUILD_TYPE} MATCHES "Debug")
  exec_program("mkdir -p ${CMAKE_BINARY_DIR}/cov")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif()

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fPIC -D_GNU_SOURCE")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")

#Set up some version-specific defines
EXECUTE_PROCESS(COMMAND git rev-parse HEAD
    OUTPUT_VARIABLE PROJECT_VERSION_GIT
    )
STRING(REGEX REPLACE "\n" "" PROJECT_VERSION_GIT ${PROJECT_VERSION_GIT})

SITE_NAME(AURA_BUILD_HOSTNAME)
TODAY(AURA_BUILD_DATE)
SET(PROJECT_BUILD_TAG "$ENV{USER}@${AURA_BUILD_HOSTNAME}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_VERSION='\"${PROJECT_VERSION}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_VERSION_GIT='\"${PROJECT_VERSION_GIT}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_BUILD_TAG='\"${PROJECT_BUILD_TAG}\"'")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_BUILD_DATE='\"${AURA_BUILD_DATE}\"'")

if(AURA_USE_BUFFER_POOL)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAURA_USE_BUFFER_POOL")
endif()


INCLUDE_DIRECTORIES(
    ./include/
    ${AURA_INCLUDE_DIRS}
)

ADD_LIBRARY(aura OBJECT ${SRCS})
if (AURA_COVERALLS)
    include(Coveralls)
    coveralls_turn_on_coverage()
    coveralls_setup(
       "${SRCS}" # The source files.
       ON                 # If we should upload.
       )
    find_program(LCOV_FOUND lcov)
    if (LCOV_FOUND)
      add_custom_target(coverage.info COMMAND geninfo ${CMAKE_BINARY_DIR} -o coverage.info)
      add_custom_target(coverage.filtered.info
                        DEPENDS coverage.info
                        COMMAND lcov --remove coverage.info
                                  '/usr/include/*'
                                  '${CMAKE_SOURCE_DIR}/tests/*'
                                -o coverage.filtered.info)
    endif()
endif()

ADD_LIBRARY(aurastatic STATIC $<TARGET_OBJECTS:aura>)
SET_TARGET_PROPERTIES(aurastatic PROPERTIES OUTPUT_NAME aura)
TARGET_LINK_LIBRARIES(aurastatic
    ${AURA_LINK_LIBS}
)

ADD_LIBRARY(aurashared SHARED $<TARGET_OBJECTS:aura>)
SET_TARGET_PROPERTIES(aurashared PROPERTIES OUTPUT_NAME aura)
TARGET_LINK_LIBRARIES(aurashared
    ${AURA_LINK_LIBS}
    -lrt -lm -lpthread
)

SET_TARGET_PROPERTIES(aurashared PROPERTIES SOVERSION ${PROJECT_VERSION}
  VERSION ${AURA_API_VERSION})


generate_clang_complete()

file(WRITE ${CMAKE_SOURCE_DIR}/.core_sources "")
foreach(f ${SRCS})
  file(APPEND ${CMAKE_SOURCE_DIR}/.core_sources "${f} ")
endforeach()


message("libaura version:           ${PROJECT_VERSION}")
message("libaura git commit:        ${PROJECT_VERSION_GIT}")
message("Enabled transports:        ${TRANSPORTS}")
message("Enabled language bindings: ${BINDINGS}")
message("Install prefix:            ${CMAKE_INSTALL_PREFIX}")
message("Event system:              ${EVENTSYSTEM}")
message("Test suites to run:        ${AURA_TEST_SUITES_TO_RUN}")
message("Test coverage upload:      ${AURA_COVERALLS}")
message("linker flags:              ${AURA_LINK_LIBS}")
if (AURA_USE_BUFFER_POOL)
  message("Buffer pooling:            enabled")
else()
  message("Buffer pooling:            disabled")
endif()

if(LUA_FOUND)
  message("lua .so dir:               ${CMAKE_INSTALL_PREFIX}/${LUA_CPATH}")
  message("lua lib dir:               ${CMAKE_INSTALL_PREFIX}/${LUA_LPATH}")
endif()

if(DOXYGEN_FOUND AND AURA_BUILD_DOC)
  message("Doxygen docs:              enabled")
else()
  message("Doxygen docs:              disabled")
endif()

if(LDOC_FOUND AND AURA_BUILD_DOC)
  message("ldoc docs:                 enabled")
else()
  message("ldoc docs:                 disabled")
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
    "-I\${includedir} -D_GNU_SOURCE"
)

CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

if (AURA_BUILD_DOC)
  file(GLOB HTMLFILES ${CMAKE_CURRENT_SOURCE_DIR}/doc-index/*)
  file(COPY ${HTMLFILES}
  DESTINATION ${CMAKE_BINARY_DIR}/doc/)

  if (DOXYGEN_FOUND)
    CONFIGURE_FILE(
      "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    )

    add_custom_command(TARGET aurashared
      PRE_BUILD
      COMMAND
      mkdir -p ${CMAKE_BINARY_DIR}/doc/C/
    )

    add_custom_command(
      TARGET aurashared
      POST_BUILD
      COMMAND doxygen > doxygen.log
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running doxygen"
    )
  endif()


  if (LDOC_FOUND)
    add_custom_command(TARGET aurashared
      PRE_BUILD
      COMMAND
      mkdir -p ${CMAKE_BINARY_DIR}/doc/lua/
    )

    add_custom_command(TARGET aurashared
                      POST_BUILD
                      COMMAND ldoc -M
                      -d ${CMAKE_BINARY_DIR}/doc/lua/${PROJECT_DOC_VERSION}/
                      lua/
                      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    COMMENT "Generating documentation for lua bindings")
  endif()
endif()

INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
       DESTINATION lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
