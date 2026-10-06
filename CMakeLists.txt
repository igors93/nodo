cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NODO_SOFTWARE_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
if(NOT NODO_SOFTWARE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic software version")
endif()

project(Nodo
    VERSION ${NODO_SOFTWARE_VERSION}
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Consensus and monetary arithmetic currently require compiler 128-bit integers.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
    "int main() { unsigned __int128 value = 1; return value != 1; }"
    NODO_HAS_UINT128)
if(NOT NODO_HAS_UINT128)
    message(FATAL_ERROR "Nodo requires a C++20 compiler with unsigned __int128 (GCC or Clang)")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(NODO_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)
if(NODO_WARNINGS_AS_ERRORS)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-Werror>
                        $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif()

include(cmake/NodoDependencies.cmake)

function(nodo_copy_mingw_runtime_dlls target_name)
    if(NOT MINGW)
        return()
    endif()

    get_filename_component(NODO_MINGW_COMPILER_DIR
        "${CMAKE_CXX_COMPILER}"
        DIRECTORY
    )

    set(NODO_MINGW_RUNTIME_DLLS
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
        libcrypto-3-x64.dll
    )

    foreach(NODO_MINGW_RUNTIME_DLL IN LISTS NODO_MINGW_RUNTIME_DLLS)
        string(MAKE_C_IDENTIFIER
            "${NODO_MINGW_RUNTIME_DLL}"
            NODO_MINGW_RUNTIME_DLL_ID
        )

        find_file(NODO_MINGW_RUNTIME_DLL_${NODO_MINGW_RUNTIME_DLL_ID}
            NAMES "${NODO_MINGW_RUNTIME_DLL}"
            HINTS
            "${NODO_MINGW_COMPILER_DIR}"
            ENV PATH
            NO_DEFAULT_PATH
        )

        if(NODO_MINGW_RUNTIME_DLL_${NODO_MINGW_RUNTIME_DLL_ID})
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${NODO_MINGW_RUNTIME_DLL_${NODO_MINGW_RUNTIME_DLL_ID}}"
                "$<TARGET_FILE_DIR:${target_name}>"
                VERBATIM
            )
        endif()
    endforeach()
endfunction()

add_library(nodo_core STATIC
    src/crypto/hash.c
)

target_include_directories(nodo_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${asio_SOURCE_DIR}/asio/include
)

target_compile_definitions(nodo_core
    PUBLIC
    ASIO_STANDALONE
    # Without an explicit target Asio warns on every Windows translation unit
    # and assumes Windows 7; Windows 10 is also MinGW's own default.
    $<$<BOOL:${WIN32}>:_WIN32_WINNT=0x0A00>
)

target_compile_options(nodo_core
    PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>
)

find_package(Threads REQUIRED)

target_link_libraries(nodo_core
    PUBLIC
    ${NODO_CRYPTO_TARGET}
    blst::blst
    Threads::Threads
    PRIVATE
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(nodo_core
        PUBLIC
        ws2_32
        mswsock # Asio's asynchronous accept on Windows
    )
endif()

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

set_target_properties(nodo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

target_link_libraries(nodo
    PRIVATE
    nodo_core
)

nodo_copy_mingw_runtime_dlls(nodo)

enable_testing()
option(NODO_TESTS_GROUPED "Link tests by module into a shared runner" ON)

function(nodo_add_test test_name test_source)
    add_executable(${test_name}
        ${test_source}
    )

    target_link_libraries(${test_name}
        PRIVATE
        nodo_core
    )
    target_compile_options(${test_name} PRIVATE -UNDEBUG)

    nodo_copy_mingw_runtime_dlls(${test_name})

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
    )
endfunction()

file(GLOB_RECURSE NODO_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

set(NODO_TEST_MODULE_KEYS)
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

    if(NODO_TESTS_GROUPED)
        if(NODO_TEST_PREFIX STREQUAL "")
            set(NODO_TEST_MODULE root)
        else()
            set(NODO_TEST_MODULE ${NODO_TEST_PREFIX})
        endif()
        list(APPEND NODO_TEST_MODULE_KEYS ${NODO_TEST_MODULE})
        set(NODO_TEST_ENTRY "nodo_test_${NODO_TEST_TARGET}")
        set_property(SOURCE ${NODO_TEST_SOURCE} PROPERTY
            COMPILE_DEFINITIONS "main=${NODO_TEST_ENTRY}")
        list(APPEND NODO_TEST_GROUP_${NODO_TEST_MODULE} ${NODO_TEST_SOURCE})
        list(APPEND NODO_TEST_NAMES_${NODO_TEST_MODULE} ${NODO_TEST_TARGET})
        string(APPEND NODO_TEST_DECL_${NODO_TEST_MODULE}
            "int ${NODO_TEST_ENTRY}();\n")
        string(APPEND NODO_TEST_DISPATCH_${NODO_TEST_MODULE}
            "  if (name == \"${NODO_TEST_TARGET}\") return ${NODO_TEST_ENTRY}();\n")
    else()
        nodo_add_test(${NODO_TEST_TARGET} ${NODO_TEST_SOURCE})
    endif()
endforeach()

if(NODO_TESTS_GROUPED)
    list(REMOVE_DUPLICATES NODO_TEST_MODULE_KEYS)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    foreach(NODO_TEST_MODULE ${NODO_TEST_MODULE_KEYS})
        set(NODO_TEST_RUNNER "${CMAKE_CURRENT_BINARY_DIR}/generated/${NODO_TEST_MODULE}_runner.cpp")
        file(WRITE "${NODO_TEST_RUNNER}"
            "#include <string_view>\n${NODO_TEST_DECL_${NODO_TEST_MODULE}}\n"
            "int main(int argc, char **argv) {\n"
            "  if (argc != 2) return 2;\n"
            "  const std::string_view name(argv[1]);\n"
            "${NODO_TEST_DISPATCH_${NODO_TEST_MODULE}}"
            "  return 2;\n}\n")
        set(NODO_TEST_GROUP_TARGET "nodo_tests_${NODO_TEST_MODULE}")
        add_executable(${NODO_TEST_GROUP_TARGET}
            ${NODO_TEST_GROUP_${NODO_TEST_MODULE}} ${NODO_TEST_RUNNER})
        target_link_libraries(${NODO_TEST_GROUP_TARGET} PRIVATE nodo_core)
        target_compile_options(${NODO_TEST_GROUP_TARGET} PRIVATE -UNDEBUG)
        nodo_copy_mingw_runtime_dlls(${NODO_TEST_GROUP_TARGET})
        foreach(NODO_TEST_NAME ${NODO_TEST_NAMES_${NODO_TEST_MODULE}})
            add_test(NAME ${NODO_TEST_NAME}
                COMMAND ${NODO_TEST_GROUP_TARGET} ${NODO_TEST_NAME})
        endforeach()
    endforeach()
endif()

if(TEST node_RealTcpThreeNodeEndToEndTests)
    if(WIN32)
        set_tests_properties(node_RealTcpThreeNodeEndToEndTests
            PROPERTIES DISABLED TRUE
        )
    else()
        set_tests_properties(node_RealTcpThreeNodeEndToEndTests
            PROPERTIES
            RUN_SERIAL TRUE
            TIMEOUT 300
        )
    endif()
endif()

# Every multi-node real-TCP end-to-end test built on
# tests/common/RealTcpNodeTestSupport.hpp spawns real OS processes bound to
# ports derived from its own PID. Two such tests running concurrently under
# ctest -j can race on the same port (or otherwise starve each other's
# handshake timing), so they must never run in parallel with each other or
# with anything else, matching node_RealTcpThreeNodeEndToEndTests above.
set(NODO_REAL_TCP_MULTI_NODE_TESTS
    node_DistributedFinalizationTests
    node_TransactionPropagationTests
    node_WrongGenesisRejectionTests
    node_ProposerFailoverTests
    node_EquivocationSlashingE2ETests
    node_GovernanceGossipE2ETests
    node_FourValidatorDevnetTests
)
foreach(NODO_REAL_TCP_TEST ${NODO_REAL_TCP_MULTI_NODE_TESTS})
    if(TEST ${NODO_REAL_TCP_TEST})
        if(WIN32)
            set_tests_properties(${NODO_REAL_TCP_TEST}
                PROPERTIES DISABLED TRUE
            )
        else()
            # 600s, not 300s: some of these tests (e.g. proposer failover,
            # equivocation slashing, governance gossip's filler-block loop)
            # sum multiple sequential waitUntil() budgets that can total
            # close to 500s in the worst case on slow/loaded runners (Clang
            # Debug, ASan+UBSan), and a ctest-level TIMEOUT firing first
            # would mask the test's own, more specific failure message.
            set_tests_properties(${NODO_REAL_TCP_TEST}
                PROPERTIES
                RUN_SERIAL TRUE
                TIMEOUT 600
            )
        endif()
    endif()
endforeach()

if(TEST node_CrossPlatformRealTcpHandshakeTests)
    set_tests_properties(node_CrossPlatformRealTcpHandshakeTests
        PROPERTIES RUN_SERIAL TRUE TIMEOUT 90)
endif()

option(NODO_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
if(NODO_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "NODO_BUILD_FUZZERS requires Clang")
    endif()
    add_executable(nodo_fuzz_network_envelope fuzz/NetworkEnvelopeFuzzer.cpp)
    target_link_libraries(nodo_fuzz_network_envelope PRIVATE nodo_core)
    target_compile_options(nodo_fuzz_network_envelope PRIVATE -fsanitize=fuzzer,address)
    target_link_options(nodo_fuzz_network_envelope PRIVATE -fsanitize=fuzzer,address)
endif()
