file(GLOB_RECURSE DBASE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(DBASE_TEST_SOURCES)
    foreach(test_src IN LISTS DBASE_TEST_SOURCES)
        get_filename_component(test_name "${test_src}" NAME_WE)

        add_executable(${test_name} "${test_src}")
        target_link_libraries(${test_name} PRIVATE dbase::dbase)

        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()