file(GLOB_RECURSE DBASE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/code/include/dbase/*.h"
    "${CMAKE_SOURCE_DIR}/code/include/dbase/*.hpp"
)

file(GLOB_RECURSE DBASE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
)

if(DBASE_BUILD_SHARED)
    add_library(dbase SHARED)
else()
    add_library(dbase STATIC)
endif()

add_library(dbase::dbase ALIAS dbase)

target_sources(dbase
    PRIVATE
        ${DBASE_SOURCES}
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_SOURCE_DIR}/code/include"
        FILES ${DBASE_HEADERS}
)

target_include_directories(dbase
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/code/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

dbase_apply_options(dbase)
dbase_apply_warnings(dbase)

target_compile_definitions(dbase
    PUBLIC
        $<$<PLATFORM_ID:Windows>:NOMINMAX>
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
        $<$<PLATFORM_ID:Windows>:_SCL_SECURE_NO_WARNINGS>
        $<$<PLATFORM_ID:Windows>:UNICODE>
        $<$<PLATFORM_ID:Windows>:_UNICODE>
        $<$<CONFIG:Debug>:DBASE_DEBUG>
)

set_target_properties(dbase PROPERTIES
    OUTPUT_NAME dbase
    DEBUG_POSTFIX d
    POSITION_INDEPENDENT_CODE ON
    VISIBILITY_INLINES_HIDDEN ON
)

if(DBASE_ENABLE_INSTALL)
    install(
        TARGETS dbase
        EXPORT dbaseTargets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()