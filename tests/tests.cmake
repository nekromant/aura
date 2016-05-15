# This function adds tests from a subdirectory unders tests/
# prefix is appended to each compiled C file
function(ADD_C_TEST_DIRECTORY prefix directory RUN MEMCHECK)
    file(GLOB UNITS
        "${CMAKE_SOURCE_DIR}/tests/${directory}/*.c"
    )

    foreach(file ${UNITS})
      GET_FILENAME_COMPONENT(f ${file} NAME_WE)
      file(APPEND ${CMAKE_SOURCE_DIR}/.core_sources "tests/${directory}/${f}.c ")
      SET(f "test-${prefix}-${f}")
      ADD_EXECUTABLE(${f} ${file})
      TARGET_LINK_LIBRARIES(${f} aurashared)
      if (${RUN})
          ADD_TEST(${f} ${f})
      endif()
      if (${MEMCHECK})
        ADD_TEST(memcheck-${f} valgrind
          --error-exitcode=1 --read-var-info=yes
          --leak-check=full  --show-leak-kinds=all
          --suppressions=${CMAKE_SOURCE_DIR}/valgrind.suppress
          --undef-value-errors=no --xml=yes --xml-file=${f}.xml
          ./${f})
      endif()
    endforeach(file)
endfunction(ADD_C_TEST_DIRECTORY)

function(ADD_SCRIPT_TEST_DIRECTORY prefix directory ext RUN MEMCHECK)
    file(GLOB UNITS
        "${CMAKE_SOURCE_DIR}/tests/${directory}/*.${ext}"
    )
    foreach(file ${UNITS})
      GET_FILENAME_COMPONENT(f ${file} NAME_WE)
      SET(f "test-${prefix}-${f}")
      if (${RUN})
          ADD_TEST(${f} ${file})
      endif()
      if (${MEMCHECK})
        ADD_TEST(memcheck-${f} valgrind
          --error-exitcode=1 --read-var-info=yes
          --leak-check=full  --show-leak-kinds=all
          --suppressions=${CMAKE_SOURCE_DIR}/valgrind.suppress
          --undef-value-errors=no --xml=yes --xml-file=${f}.xml
          ${file})
      endif()
    endforeach(file)
endfunction(ADD_SCRIPT_TEST_DIRECTORY)
