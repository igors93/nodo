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

if(NOT TARGET blst::blst)
    add_library(blst::blst UNKNOWN IMPORTED)
    set_target_properties(blst::blst
        PROPERTIES
            IMPORTED_LOCATION "${BLST_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BLST_INCLUDE_DIR}"
    )
endif()

# Standalone Asio Dependency
include(FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio)

