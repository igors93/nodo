cmake_minimum_required(VERSION 3.20)

project(Nodo
    VERSION 0.1.0
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

find_package(OpenSSL REQUIRED COMPONENTS Crypto)

set(NODO_BLST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/blst")

if(EXISTS "${NODO_BLST_SOURCE_DIR}/src/server.c" AND
   EXISTS "${NODO_BLST_SOURCE_DIR}/bindings/blst.h")
    add_library(blst STATIC
        "${NODO_BLST_SOURCE_DIR}/src/server.c"
        "${NODO_BLST_SOURCE_DIR}/build/assembly.S"
    )

    target_include_directories(blst
        PUBLIC
            "${NODO_BLST_SOURCE_DIR}/bindings"
    )

    target_compile_options(blst
        PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra>
    )
else()
    find_path(BLST_INCLUDE_DIR blst.h)
    find_library(BLST_LIBRARY blst)

    if(NOT BLST_INCLUDE_DIR OR NOT BLST_LIBRARY)
        message(FATAL_ERROR
            "Nodo requires real BLS12-381 support through blst. "
            "Install blst development headers/library or vendor blst at third_party/blst. "
            "Fake or development BLS providers are not allowed."
        )
    endif()

    add_library(blst STATIC IMPORTED)
    set_target_properties(blst
        PROPERTIES
            IMPORTED_LOCATION "${BLST_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BLST_INCLUDE_DIR}"
    )
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

target_link_libraries(nodo_core
    PUBLIC
        OpenSSL::Crypto
        blst
)

file(GLOB_RECURSE NODO_CORE_CPP_SOURCES
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

file(GLOB_RECURSE NODO_TEST_SOURCES
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
