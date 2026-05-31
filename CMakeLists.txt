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

find_package(PkgConfig QUIET)

find_package(OpenSSL QUIET COMPONENTS Crypto)
if(OpenSSL_FOUND AND TARGET OpenSSL::Crypto)
    set(NODO_CRYPTO_TARGET OpenSSL::Crypto)
elseif(PkgConfig_FOUND)
    pkg_check_modules(NODO_LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto)
    set(NODO_CRYPTO_TARGET PkgConfig::NODO_LIBCRYPTO)
else()
    message(FATAL_ERROR
        "Nodo requires OpenSSL libcrypto for SHA-256 and Ed25519. "
        "Install OpenSSL development files or provide libcrypto through pkg-config."
    )
endif()

if(PkgConfig_FOUND)
    pkg_check_modules(BLST_PKG QUIET blst)
endif()

set(BLST_ROOT "" CACHE PATH "Root directory for an external blst installation")
set(NODO_BLST_HINTS)

if(BLST_ROOT)
    list(APPEND NODO_BLST_HINTS "${BLST_ROOT}")
endif()

if(DEFINED ENV{BLST_ROOT})
    list(APPEND NODO_BLST_HINTS "$ENV{BLST_ROOT}")
endif()

if(DEFINED ENV{HOME})
    list(APPEND NODO_BLST_HINTS "$ENV{HOME}/.local")
endif()

if(BLST_INCLUDE_DIR AND NOT EXISTS "${BLST_INCLUDE_DIR}/blst.h")
    unset(BLST_INCLUDE_DIR CACHE)
endif()

if(BLST_LIBRARY AND NOT EXISTS "${BLST_LIBRARY}")
    unset(BLST_LIBRARY CACHE)
endif()

find_path(BLST_INCLUDE_DIR
    NAMES blst.h
    HINTS
        ${BLST_PKG_INCLUDE_DIRS}
        ${NODO_BLST_HINTS}
    PATH_SUFFIXES
        include
        include/blst
        bindings
)

find_library(BLST_LIBRARY
    NAMES blst libblst
    HINTS
        ${BLST_PKG_LIBRARY_DIRS}
        ${NODO_BLST_HINTS}
    PATH_SUFFIXES
        lib
        lib64
)

if(NOT BLST_INCLUDE_DIR OR NOT EXISTS "${BLST_INCLUDE_DIR}/blst.h"
        OR NOT BLST_LIBRARY OR NOT EXISTS "${BLST_LIBRARY}")
    message(FATAL_ERROR
        "Nodo requires external blst for BLS12-381. "
        "Install blst development headers/library so CMake can find blst.h and libblst. "
        "The Nodo repository must not vendor third_party/blst, and fake BLS providers are not allowed."
    )
endif()

add_library(blst::blst UNKNOWN IMPORTED)
set_target_properties(blst::blst
    PROPERTIES
        IMPORTED_LOCATION "${BLST_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BLST_INCLUDE_DIR}"
)

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
        ${NODO_CRYPTO_TARGET}
        blst::blst
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

set_target_properties(nodo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

target_link_libraries(nodo
    PRIVATE
        nodo_core
)

enable_testing()

function(nodo_add_test test_name test_source)
    add_executable(${test_name}
        ${test_source}
    )

    target_link_libraries(${test_name}
        PRIVATE
            nodo_core
    )

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endfunction()

file(GLOB_RECURSE NODO_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

foreach(NODO_TEST_SOURCE ${NODO_TEST_SOURCES})
    get_filename_component(NODO_TEST_NAME ${NODO_TEST_SOURCE} NAME_WE)
    get_filename_component(NODO_TEST_DIR ${NODO_TEST_SOURCE} DIRECTORY)
    file(RELATIVE_PATH NODO_TEST_REL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${NODO_TEST_DIR}")
    string(REPLACE "/" "_" NODO_TEST_PREFIX "${NODO_TEST_REL_DIR}")

    if(NODO_TEST_PREFIX STREQUAL "")
        set(NODO_TEST_TARGET "${NODO_TEST_NAME}")
    else()
        set(NODO_TEST_TARGET "${NODO_TEST_PREFIX}_${NODO_TEST_NAME}")
    endif()

    nodo_add_test(${NODO_TEST_TARGET} ${NODO_TEST_SOURCE})
endforeach()
