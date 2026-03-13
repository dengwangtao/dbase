include(Catch)

file(GLOB_RECURSE DBASE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

if(NOT DBASE_TEST_SOURCES)
    message(STATUS "No test sources found under ${CMAKE_CURRENT_SOURCE_DIR}")
    return()
endif()

add_executable(dbase_tests ${DBASE_TEST_SOURCES})

target_link_libraries(dbase_tests
    PRIVATE
        dbase::dbase
        Catch2::Catch2WithMain
)

target_compile_features(dbase_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_definitions(dbase_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(dbase_tests PRIVATE -O0 -g3)
endif()

catch_discover_tests(dbase_tests)