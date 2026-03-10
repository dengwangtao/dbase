file(GLOB_RECURSE DBASE_BENCHMARK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

foreach(bench_src IN LISTS DBASE_BENCHMARK_SOURCES)
    get_filename_component(bench_name "${bench_src}" NAME_WE)

    add_executable(${bench_name} "${bench_src}")
    target_link_libraries(${bench_name} PRIVATE dbase::dbase)
endforeach()