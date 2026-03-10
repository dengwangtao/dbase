cmake_minimum_required(VERSION 3.24)

project(
    dbase
    VERSION 0.1.0
    DESCRIPTION "Cross-platform C++20 base library"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
message(STATUS "PROJ_ROOT: ${PROJ_ROOT}")

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/utf-8)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_SOURCE_DIR}/bin/${OUTPUTCONFIG}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_SOURCE_DIR}/lib/${OUTPUTCONFIG}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} "${CMAKE_SOURCE_DIR}/lib/${OUTPUTCONFIG}")
endforeach()

option(DBASE_BUILD_SHARED "Build shared library" OFF)
option(DBASE_BUILD_TESTS "Build tests" ON)
option(DBASE_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(DBASE_BUILD_EXAMPLES "Build examples" ON)
option(DBASE_ENABLE_INSTALL "Enable install rules" ON)
option(DBASE_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(DBASE_ENABLE_WERROR "Treat warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include(ProjectOptions)
include(ProjectWarnings)
include(ProjectUtils)

dbase_print_platform_info()
dbase_setup_global_options()

add_subdirectory(code/src)

if(DBASE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(DBASE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(DBASE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(DBASE_ENABLE_INSTALL)
    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/code/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(
        EXPORT dbaseTargets
        FILE dbaseTargets.cmake
        NAMESPACE dbase::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dbase"
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DbaseConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/dbaseConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dbase"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/dbaseConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/dbaseConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/dbaseConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/dbase"
    )
endif()