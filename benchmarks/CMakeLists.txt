file(GLOB_RECURSE DBASE_BENCHMARK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

if(NOT DBASE_BENCHMARK_SOURCES)
    message(STATUS "No benchmark sources found under ${CMAKE_CURRENT_SOURCE_DIR}")
    return()
endif()

add_executable(dbase_benchmarks ${DBASE_BENCHMARK_SOURCES})

target_link_libraries(dbase_benchmarks
    PRIVATE
        dbase::dbase
        benchmark::benchmark
        Threads::Threads
)

target_compile_features(dbase_benchmarks PRIVATE cxx_std_20)

if(MSVC)
    target_compile_definitions(dbase_benchmarks PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(dbase_benchmarks PRIVATE -O3 -DNDEBUG)
endif()