# This function adds tests from a subdirectory unders tests/
# prefix is appended to each compiled C file
function(add_aura_test testname testcommand)
    set(list_var "${ARGN}")

    # If we run tests on remote hardware, rewrite the testcommand to invoke
    # ssh. The remote should have passwordless auth
    if (AURA_TEST_REMOTE)
        SET(testcommand ssh ${AURA_TEST_REMOTE_HOST} ./${testcommand})
    endif()

    ADD_TEST(epoll-${testname} ${testcommand} ${ARGN})
    set_property(TEST epoll-${testname} PROPERTY ENVIRONMENT "AURA_USE_EVENTLOOP=epoll" "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua")
    set_property(TEST epoll-${testname} PROPERTY TIMEOUT 30)
    if(LIBEVENT_FOUND)
        ADD_TEST(libevent-${testname} ${testcommand} ${ARGN})
        set_property(TEST libevent-${testname} PROPERTY ENVIRONMENT "AURA_USE_EVENTLOOP=libevent" "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua")
        set_property(TEST libevent-${testname} PROPERTY TIMEOUT 30)
    endif()
endfunction(add_aura_test)

function(ADD_C_TEST_DIRECTORY prefix directory RUN MEMCHECK)
    file(GLOB UNITS
        "${CMAKE_SOURCE_DIR}/tests/${directory}/*.c"
    )

    if (${RUN})
        SET(AURA_TEST_SUITES_TO_RUN "${AURA_TEST_SUITES_TO_RUN}${prefix} " PARENT_SCOPE)
        if (${MEMCHECK})
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
          add_aura_test(${f} ${f})
        if (${MEMCHECK})
          add_aura_test(memcheck-${f} valgrind
            --error-exitcode=1 --read-var-info=yes
            --leak-check=full  --show-leak-kinds=all
            --suppressions=${CMAKE_SOURCE_DIR}/valgrind.suppress
            --undef-value-errors=no --xml=yes --xml-file=${f}.xml
            ./${f})
        endif()
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
          add_aura_test(${f} ./lua-test-wrapper ${file})
          if (${MEMCHECK})
              add_aura_test(memcheck-${f} valgrind
                  --error-exitcode=1 --read-var-info=yes
                  --leak-check=full  --show-leak-kinds=all
                  --suppressions=${CMAKE_SOURCE_DIR}/valgrind.suppress
                  --undef-value-errors=no --xml=yes --xml-file=${f}.xml
              ./lua-test-wrapper ${file})
          endif()
      endif()
    endforeach(file)
endfunction(ADD_SCRIPT_TEST_DIRECTORY)


file(WRITE ${CMAKE_BINARY_DIR}/.bashrc "export AURA_BUILDDIR=${CMAKE_BINARY_DIR}" \n)
file(APPEND ${CMAKE_BINARY_DIR}/.bashrc "export AURA_SOURCEDIR=${CMAKE_SOURCE_DIR}" \n)
