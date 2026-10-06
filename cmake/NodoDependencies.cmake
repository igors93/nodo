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

set(BLST_ROOT "" CACHE PATH "Root directory for an external blst installation")

set(NODO_BLST_PREFIX_HINTS)

if(BLST_ROOT)
    list(APPEND NODO_BLST_PREFIX_HINTS "${BLST_ROOT}")
endif()

if(DEFINED ENV{BLST_ROOT} AND NOT "$ENV{BLST_ROOT}" STREQUAL "")
    list(APPEND NODO_BLST_PREFIX_HINTS "$ENV{BLST_ROOT}")
endif()

if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    list(APPEND NODO_BLST_PREFIX_HINTS
        "$ENV{HOME}/.nodo/deps/blst"
        "$ENV{HOME}/.local/nodo/deps/blst"
    )
endif()

unset(BLST_INCLUDE_DIR)
unset(BLST_INCLUDE_DIR CACHE)
unset(BLST_LIBRARY)
unset(BLST_LIBRARY CACHE)

find_path(BLST_INCLUDE_DIR
    NAMES blst.h
    HINTS ${NODO_BLST_PREFIX_HINTS}
    PATH_SUFFIXES
    include
    include/blst
    bindings
    NO_DEFAULT_PATH
)

find_library(BLST_LIBRARY
    NAMES blst libblst
    HINTS ${NODO_BLST_PREFIX_HINTS}
    PATH_SUFFIXES
    lib
    lib64
    .
    NO_DEFAULT_PATH
)

if(NOT BLST_INCLUDE_DIR OR NOT BLST_LIBRARY)
    find_path(BLST_INCLUDE_DIR
        NAMES blst.h
        PATH_SUFFIXES
        include
        include/blst
        bindings
    )

    find_library(BLST_LIBRARY
        NAMES blst libblst
        PATH_SUFFIXES
        lib
        lib64
    )
endif()

if((NOT BLST_INCLUDE_DIR OR NOT BLST_LIBRARY) AND PkgConfig_FOUND)
    pkg_check_modules(BLST_PKG QUIET blst)

    if(BLST_PKG_FOUND)
        find_path(BLST_INCLUDE_DIR
            NAMES blst.h
            HINTS ${BLST_PKG_INCLUDE_DIRS}
            NO_DEFAULT_PATH
        )

        find_library(BLST_LIBRARY
            NAMES blst libblst
            HINTS ${BLST_PKG_LIBRARY_DIRS}
            NO_DEFAULT_PATH
        )
    endif()
endif()

if(NOT BLST_INCLUDE_DIR OR NOT EXISTS "${BLST_INCLUDE_DIR}/blst.h"
    OR NOT BLST_LIBRARY OR NOT EXISTS "${BLST_LIBRARY}")
    message(FATAL_ERROR
        "Nodo requires external blst for real BLS12-381 signatures, but CMake could not find blst.h and libblst. "
        "Do not place blst inside the Nodo repository and do not create third_party/blst. "
        "Install it outside the project with scripts/install_blst.sh, which installs to ~/.nodo/deps/blst, "
        "or configure CMake with -DBLST_ROOT=/path/to/blst."
    )
endif()

# blst does not publish a version macro. Check the public headers against the
# audited v0.3.11 release so an arbitrary system installation cannot silently
# satisfy the dependency. The library must be installed from the same release.
file(SHA256 "${BLST_INCLUDE_DIR}/blst.h" NODO_BLST_HEADER_SHA256)
file(SHA256 "${BLST_INCLUDE_DIR}/blst_aux.h" NODO_BLST_AUX_SHA256)
if(NOT NODO_BLST_HEADER_SHA256 STREQUAL
        "e337c942c5b5ac4565048b39fd6d432ba6923a14d59d46e7dee43d664015c441"
    OR NOT NODO_BLST_AUX_SHA256 STREQUAL
        "af001093f836f0ad258b567444e1b066cd73392d2e1e982b6de5a9659c01be37")
    message(FATAL_ERROR "blst headers must match pinned v0.3.11; reinstall with scripts/install_blst.sh")
endif()

if(NOT TARGET blst::blst)
    add_library(blst::blst UNKNOWN IMPORTED)
    set_target_properties(blst::blst
        PROPERTIES
        IMPORTED_LOCATION "${BLST_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${BLST_INCLUDE_DIR}"
    )
endif()

# Pass local source directories for an offline build. CMake's FetchContent
# source-dir overrides also work with FETCHCONTENT_FULLY_DISCONNECTED=ON.
set(NODO_ASIO_SOURCE_DIR "" CACHE PATH "Local Asio source tree")
set(NODO_JSON_SOURCE_DIR "" CACHE PATH "Local nlohmann/json source tree")
if(NODO_ASIO_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_ASIO "${NODO_ASIO_SOURCE_DIR}")
endif()
if(NODO_JSON_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${NODO_JSON_SOURCE_DIR}")
endif()

# Standalone Asio Dependency
include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG 12e0ce9e0500bf0f247dbd1ae894272656456079
)
FetchContent_MakeAvailable(asio)

# nlohmann/json parses untrusted JSON-RPC input. The release tarball is pinned
# by SHA-256 so a moved tag or a tampered download fails the configure step.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(JSON_ImplicitConversions OFF CACHE INTERNAL "Require explicit nlohmann::json conversions")
set(JSON_SystemInclude ON CACHE INTERNAL "Treat nlohmann::json headers as system headers")
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_MakeAvailable(nlohmann_json)

