enable_testing()
set(LINK_LIBRARIES simple_leveldb gtest gtest_main)

# 启用测试
enable_testing()

set(TESTS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/tests")

file(MAKE_DIRECTORY ${TESTS_OUTPUT_DIR})

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

foreach(TEST_FILE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    set(TARGET_NAME "test_${TEST_NAME}")

    add_executable(${TARGET_NAME} ${TEST_FILE})

    target_link_libraries(${TARGET_NAME} ${LINK_LIBRARIES})

    set_target_properties(${TARGET_NAME} PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY ${TESTS_OUTPUT_DIR})

    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endforeach()
