# This function adds tests from a subdirectory unders tests/
# prefix is appended to each compiled C file
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
            ADD_TEST(${f} ${f})
            set_property(TEST ${f} PROPERTY ENVIRONMENT "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua")
        if (${MEMCHECK})
          ADD_TEST(memcheck-${f} valgrind
            --error-exitcode=1 --read-var-info=yes
            --leak-check=full  --show-leak-kinds=all
            --suppressions=${CMAKE_SOURCE_DIR}/valgrind.suppress
            --undef-value-errors=no --xml=yes --xml-file=${f}.xml
            ./${f})
            set_property(TEST memcheck-${f} PROPERTY ENVIRONMENT "AURA_LUA_SCRIPT_PATH=${CMAKE_SOURCE_DIR}/lua")
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
          ADD_TEST(${f} ./lua-test-wrapper ${file})
          if (${MEMCHECK})
              ADD_TEST(memcheck-${f} valgrind
                  --error-exitcode=1 --read-var-info=yes
                  --leak-check=full  --show-leak-kinds=all
                  --suppressions=${CMAKE_SOURCE_DIR}/valgrind.suppress
                  --undef-value-errors=no --xml=yes --xml-file=${f}.xml
              ./lua-test-wrapper ${file})
          endif()
      endif()
    endforeach(file)
endfunction(ADD_SCRIPT_TEST_DIRECTORY)
