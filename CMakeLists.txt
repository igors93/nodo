cmake_minimum_required(VERSION 3.20)

project(Nodo
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_library(nodo_core STATIC
    src/crypto/hash.c
)

target_include_directories(nodo_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(nodo_core
    PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>
)

file(GLOB_RECURSE NODO_CORE_CPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

target_sources(nodo_core
    PRIVATE
        ${NODO_CORE_CPP_SOURCES}
)

add_executable(nodo
    apps/cli/main.cpp
)

target_link_libraries(nodo
    PRIVATE
        nodo_core
)

set_target_properties(nodo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

enable_testing()

file(GLOB_RECURSE NODO_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

foreach(test_source ${NODO_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    get_filename_component(test_dir ${test_source} DIRECTORY)
    file(RELATIVE_PATH test_relative_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${test_dir}")
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" test_target "${test_name}")

    add_executable(${test_target}
        ${test_source}
    )

    target_link_libraries(${test_target}
        PRIVATE
            nodo_core
    )

    target_include_directories(${test_target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    set_target_properties(${test_target}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/${test_relative_dir}"
    )

    add_test(
        NAME ${test_relative_dir}/${test_target}
        COMMAND ${test_target}
    )

    if(test_relative_dir STREQUAL "economics")
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "economics;protection"
        )
    elseif(test_relative_dir STREQUAL "storage")
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "storage"
        )
    elseif(test_relative_dir STREQUAL "core")
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "core;protection"
        )
    elseif(test_relative_dir STREQUAL "crypto")
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "crypto"
        )
    elseif(test_relative_dir STREQUAL "consensus")
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "consensus;protection"
        )
    elseif(test_relative_dir STREQUAL "mempool")
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "mempool"
        )
    else()
        set_tests_properties(${test_relative_dir}/${test_target}
            PROPERTIES LABELS "${test_relative_dir}"
        )
    endif()
endforeach()
