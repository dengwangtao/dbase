function(dbase_setup_global_options)
endfunction()

function(dbase_apply_options target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PUBLIC
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /EHsc
            /bigobj
        )
    else()
        target_compile_options(${target_name} PUBLIC
            -fPIC
        )
    endif()

    if(WIN32)
        target_compile_definitions(${target_name} PUBLIC
            _CRT_SECURE_NO_WARNINGS
            _SCL_SECURE_NO_WARNINGS
            UNICODE
            _UNICODE
        )
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${target_name} PUBLIC
            pthread
            dl
        )
    endif()

    if(APPLE)
        find_library(FOUNDATION_FRAMEWORK Foundation)
        if(FOUNDATION_FRAMEWORK)
            target_link_libraries(${target_name} PUBLIC
                ${FOUNDATION_FRAMEWORK}
            )
        endif()
    endif()
endfunction()