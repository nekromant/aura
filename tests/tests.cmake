# This function adds tests from a subdirectory unders tests/
# prefix is appended to each compiled C file


function(add_test_with_valgrind testname testtimeout testenv testcommand memcheck testargs)
    # If we run tests on remote hardware, rewrite the testcommand to invoke
    # ssh. The remote should have passwordless auth

    message("test ${testname} timeout ${testtimeout} cmd ${testcommand}")

    if (AURA_TEST_REMOTE)
        SET(testcommand ssh ${AURA_TEST_REMOTE_HOST} ${testcommand})
    endif()

    set(VALGRIND_OPTS --error-exitcode=1 --read-var-info=yes
          --leak-check=full ${AURA_EXTRA_VALGRIND_OPTS}
          --suppressions=valgrind.suppress
          --undef-value-errors=no --xml=yes --xml-file=${testname}.xml
    )

    ADD_TEST(${testname} ${testcommand} ${testargs})
    set_property(TEST ${testname} PROPERTY ENVIRONMENT ${testenv})
    set_property(TEST ${testname} PROPERTY TIMEOUT "${testtimeout}")

    if (false)
        ADD_TEST(memcheck-${testname} valgrind ${VALGRIND_OPTS} ${testcommand})
        set_property(TEST memcheck-${testname} PROPERTY ENVIRONMENT ${testenv})
        set_property(TEST memcheck-${testname} PROPERTY TIMEOUT ${testtimeout})
    endif()
endfunction()


function(add_aura_test testname memcheck testcommand testargs)
    SET(testname "${AURA_TEST_PREFIX}-${testname}")
    message(${testcommand})

    SET(testenv ${CTEST_ENVIRONMENT} "AURA_USE_EVENTLOOP=epoll" "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua")
    add_test_with_valgrind(epoll-${testname} "${AURA_TEST_TIMEOUT}" "${testenv}" "${testcommand}" "${memcheck}" "${testargs}")

    if(LIBEVENT_FOUND)
        SET(testenv ${CTEST_ENVIRONMENT} "AURA_USE_EVENTLOOP=epoll" "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua")
        add_test_with_valgrind(libevent-${testname} ${AURA_TEST_TIMEOUT} "${testenv}" "${testcommand}" "${memcheck}" "${testargs}")
    endif()
endfunction(add_aura_test)

function(ADD_C_TEST_DIRECTORY prefix directory RUN MEMCHECK)
    file(GLOB UNITS
        "${CMAKE_SOURCE_DIR}/tests/${directory}/*.c"
    )

    if (${RUN})
        SET(AURA_TEST_SUITES_TO_RUN "${AURA_TEST_SUITES_TO_RUN}${prefix} " PARENT_SCOPE)
        if (MEMCHECK)
            SET(AURA_TEST_SUITES_TO_RUN "${AURA_TEST_SUITES_TO_RUN}${prefix}(+memcheck) " PARENT_SCOPE)
        endif()
    endif()

    foreach(file ${UNITS})
      GET_FILENAME_COMPONENT(f ${file} NAME_WE)
      file(APPEND ${CMAKE_SOURCE_DIR}/.core_sources "tests/${directory}/${f}.c ")
      SET(f "test-${prefix}-${f}")
      ADD_EXECUTABLE(${f} ${file})
      TARGET_LINK_LIBRARIES(${f} aurashared)
      if (${RUN})
          add_aura_test(${f} ${MEMCHECK} ./${f} "")
      endif()
      INSTALL(TARGETS ${f}
        RUNTIME DESTINATION lib/${CMAKE_LIBRARY_PATH}/aura/tests/)
    endforeach(file)
endfunction(ADD_C_TEST_DIRECTORY)

add_executable(lua-test-wrapper lua-test-wrapper.c utils-lua.c)
target_link_libraries(lua-test-wrapper ${LUA_LIBRARIES})

function(ADD_SCRIPT_TEST_DIRECTORY prefix directory ext RUN MEMCHECK)
    file(GLOB UNITS
        "${CMAKE_SOURCE_DIR}/tests/${directory}/*.${ext}"
    )
    if (${RUN})
        SET(AURA_TEST_SUITES_TO_RUN "${AURA_TEST_SUITES_TO_RUN}${prefix} " PARENT_SCOPE)
        if (${MEMCHECK})
            SET(AURA_TEST_SUITES_TO_RUN "${AURA_TEST_SUITES_TO_RUN}${prefix}(+memcheck) " PARENT_SCOPE)
        endif()
    endif()

    foreach(file ${UNITS})
      GET_FILENAME_COMPONENT(f ${file} NAME_WE)
      SET(f "test-${prefix}-${f}")
      if (${RUN})
          add_aura_test(${f} "${MEMCHECK}" "./lua-test-wrapper" ${file})
      endif()
    endforeach(file)
endfunction(ADD_SCRIPT_TEST_DIRECTORY)


file(WRITE ${CMAKE_BINARY_DIR}/.bashrc "export AURA_BUILDDIR=${CMAKE_BINARY_DIR}\n")
file(APPEND ${CMAKE_BINARY_DIR}/.bashrc "export AURA_SOURCEDIR=${CMAKE_SOURCE_DIR}\n")
file(COPY ${CMAKE_SOURCE_DIR}/valgrind.suppress DESTINATION ${CMAKE_BINARY_DIR})
