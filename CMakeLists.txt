cmake_minimum_required(VERSION 3.15)

project(RiaG VERSION 0.22.0 LANGUAGES CXX C)

option(SINGLE_THREADED "Single-threaded mode" FALSE)
if(SINGLE_THREADED)
    set(__SINGLE_THREADED__ TRUE)
    add_compile_definitions(__SINGLE_THREADED__)
    message(STATUS "Single-threaded mode is enabled")
else()
    message(STATUS "Multi-threaded mode is enabled: CMAKE_BUILD_PARALLEL_LEVEL=$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
    find_package(Threads REQUIRED)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_FIND_PACKAGE_RESOLVE_SYMLINKS TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# When building inside a pixi/conda env (CI uses setup-pixi with
# activate-environment: true, setting $CONDA_PREFIX), stage the env as a CMake
# search prefix. find_package(Arrow) already happens to resolve libarrow's
# ArrowConfig.cmake under <env>/lib/cmake, but module-based probes such as
# FindOpenMP (find_library/find_path for llvm-openmp's libomp) never see that
# prefix and fail. Prepending the env here makes every find_* consult it, so
# new deps just need a pixi.toml entry - no per-lib HINTS (cf. extension/adbc).
if(DEFINED ENV{CONDA_PREFIX} AND EXISTS "$ENV{CONDA_PREFIX}")
    # Normalize backslashes to forward slashes so the path survives being
    # round-tripped through CMake's set(... "...") parser. try_compile's scratch
    # file embeds CMAKE_*_PATH entries in a set() call, and CMake 3.31 rejects
    # unrecognized escape sequences like "\a", "\b", etc. that appear in Windows
    # pixi paths (e.g. D:\a\...\.pixi\envs\default). On non-Windows this is a no-op.
    file(TO_CMAKE_PATH "$ENV{CONDA_PREFIX}" _riag_conda_prefix)
    list(PREPEND CMAKE_PREFIX_PATH "${_riag_conda_prefix}")
    list(PREPEND CMAKE_MODULE_PATH "${_riag_conda_prefix}/lib/cmake")
    if(WIN32 AND EXISTS "$ENV{CONDA_PREFIX}/Library")
        file(TO_CMAKE_PATH "$ENV{CONDA_PREFIX}/Library" _riag_conda_lib)
        list(PREPEND CMAKE_PREFIX_PATH "${_riag_conda_lib}")
    endif()
    message(STATUS "Using pixi/conda env for dependencies: ${_riag_conda_prefix}")
endif()
# On Linux, symbols in executables are not accessible by loaded shared libraries (e.g. via dlopen(3)). However, we need to export public symbols in executables so that extensions can access public symbols. This enables that behaviour.
set(CMAKE_ENABLE_EXPORTS TRUE)

# When building tests, we need all symbols visible so tests can link to the shared library
# instead of static linking (which bloats binary sizes significantly)
option(BUILD_TESTS "Build C++ tests." FALSE)
option(BUILD_EXTENSION_TESTS "Build C++ extension tests." FALSE)
if(BUILD_TESTS OR BUILD_EXTENSION_TESTS)
    set(CMAKE_CXX_VISIBILITY_PRESET default)
    set(CMAKE_C_VISIBILITY_PRESET default)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()

option(ENABLE_WERROR "Treat all warnings as errors" FALSE)
if(ENABLE_WERROR)
    if (CMAKE_VERSION VERSION_GREATER "3.24.0" OR CMAKE_VERSION VERSION_EQUAL "3.24.0")
        set(CMAKE_COMPILE_WARNING_AS_ERROR TRUE)
    elseif (MSVC)
        add_compile_options(\WX)
    else ()
        add_compile_options(-Werror)
    endif()
endif()

# Detect OS and architecture, copied from DuckDB
set(OS_NAME "unknown")
set(OS_ARCH "amd64")

set(CMAKE_TARGET_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
if(MSVC AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
  set(CMAKE_TARGET_PROCESSOR "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()
string(TOLOWER "${CMAKE_TARGET_PROCESSOR}" CMAKE_TARGET_PROCESSOR_LOWER)
string(REGEX MATCH "(arm64|aarch64)" IS_ARM "${CMAKE_TARGET_PROCESSOR_LOWER}")
if(IS_ARM)
  set(OS_ARCH "arm64")
elseif(FORCE_32_BIT)
  set(OS_ARCH "i386")
endif()

if(APPLE)
  set(OS_NAME "osx")
endif()
if(WIN32)
  set(OS_NAME "windows")
  # Prevent Windows min/max macros from breaking std::min/std::max (needed for Clang-on-Windows too)
  add_compile_definitions(NOMINMAX)
  # Required for Windows SDK headers (winnt.h) when using Clang; MSVC branch sets the target arch later.
  if(OS_ARCH STREQUAL "arm64")
    add_compile_definitions(_ARM64_)
    add_compile_definitions(PCG_LITTLE_ENDIAN=1)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(_AMD64_)
  else()
    add_compile_definitions(_X86_)
  endif()
endif()
if(UNIX AND NOT APPLE)
  set(OS_NAME "linux") # sorry BSD
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "64-bit architecture detected")
    add_compile_definitions(__64BIT__)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(STATUS "32-bit architecture detected")
    add_compile_definitions(__32BIT__)
    set(__32BIT__ TRUE)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(DEFINED ENV{PYBIND11_PYTHON_VERSION})
    set(PYBIND11_PYTHON_VERSION $ENV{PYBIND11_PYTHON_VERSION})
endif()

if(DEFINED ENV{PYTHON_EXECUTABLE})
    set(PYTHON_EXECUTABLE $ENV{PYTHON_EXECUTABLE})
endif()

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache found and enabled")
else ()
    find_program(CCACHE_PROGRAM sccache)
    if (CCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        message(STATUS "sccache found and enabled")
    endif ()
endif ()

set(INSTALL_LIB_DIR
        lib
        CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR
        bin
        CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR
        include
        CACHE PATH "Installation directory for header files")
set(INSTALL_CMAKE_DIR
        ${DEF_INSTALL_CMAKE_DIR}
        CACHE PATH "Installation directory for CMake files")

option(ENABLE_ADDRESS_SANITIZER "Enable address sanitizer." FALSE)
option(ENABLE_THREAD_SANITIZER "Enable thread sanitizer." FALSE)
option(ENABLE_UBSAN "Enable undefined behavior sanitizer." FALSE)
option(ENABLE_RUNTIME_CHECKS "Enable runtime coherency checks (e.g. asserts)" FALSE)
option(ENABLE_LTO "Enable Link-Time Optimization" FALSE)
option(ENABLE_MALLOC_BUFFER_MANAGER "Enable Buffer manager using malloc. Default option for webassembly" OFF)
option(RIAG_ENABLE_SIMD "Enable runtime-dispatched query-processor SIMD kernels." ON)

option(RIAG_DEFAULT_REL_STORAGE_DIRECTION "Only store fwd direction in rel tables by default." BOTH)
if(NOT RIAG_DEFAULT_REL_STORAGE_DIRECTION)
    set(RIAG_DEFAULT_REL_STORAGE_DIRECTION BOTH)
endif()
set(RIAG_DEFAULT_REL_STORAGE_DIRECTION ${RIAG_DEFAULT_REL_STORAGE_DIRECTION}_REL_STORAGE)
option(RIAG_PAGE_SIZE_LOG2 "Log2 of the page size." 12)
if(NOT RIAG_PAGE_SIZE_LOG2)
    set(RIAG_PAGE_SIZE_LOG2 12)
endif()
message(STATUS "RIAG_PAGE_SIZE_LOG2: ${RIAG_PAGE_SIZE_LOG2}")
option(RIAG_VECTOR_CAPACITY_LOG2 "Log2 of the vector capacity." 11)
if(NOT RIAG_VECTOR_CAPACITY_LOG2)
    set(RIAG_VECTOR_CAPACITY_LOG2 11)
endif()
message(STATUS "RIAG_VECTOR_CAPACITY_LOG2: ${RIAG_VECTOR_CAPACITY_LOG2}")

# 64 * 2048 nodes per group
option(RIAG_NODE_GROUP_SIZE_LOG2 "Log2 of the vector capacity." 17)
if(NOT RIAG_NODE_GROUP_SIZE_LOG2)
    set(RIAG_NODE_GROUP_SIZE_LOG2 17)
endif()
message(STATUS "RIAG_NODE_GROUP_SIZE_LOG2: ${RIAG_NODE_GROUP_SIZE_LOG2}")

option(RIAG_MAX_SEGMENT_SIZE_LOG2 "Log2 of the maximum segment size in bytes." 18)
if(NOT RIAG_MAX_SEGMENT_SIZE_LOG2)
    set(RIAG_MAX_SEGMENT_SIZE_LOG2 18)
endif()
message(STATUS "RIAG_MAX_SEGMENT_SIZE_LOG2: ${RIAG_MAX_SEGMENT_SIZE_LOG2}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/system_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/include/common/system_config.h @ONLY)

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(F_FULLFSYNC "fcntl.h" HAS_FULLFSYNC)
check_cxx_symbol_exists(fdatasync "unistd.h" HAS_FDATASYNC)
if(HAS_FULLFSYNC)
    message(STATUS "✓ F_FULLFSYNC will be used on this platform")
    add_compile_definitions(HAS_FULLFSYNC)
else()
    message(STATUS "✗ F_FULLFSYNC not available")
endif()

if(HAS_FDATASYNC)
    message(STATUS "✓ fdatasync will be used on this platform")
    add_compile_definitions(HAS_FDATASYNC)
else()
    message(STATUS "✗ fdatasync not available, using fsync fallback")
endif()

if(MSVC)
    # Required for M_PI on Windows
    add_compile_definitions(_USE_MATH_DEFINES)
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(SERD_STATIC)
    # This is a workaround for regex oom issue on windows in gtest.
    add_compile_definitions(_REGEX_MAX_STACK_COUNT=0)
    add_compile_definitions(_REGEX_MAX_COMPLEXITY_COUNT=0)
    # Disable constexpr mutex constructor to avoid compatibility issues with
    # older versions of the MSVC runtime library
    # See: https://github.com/microsoft/STL/wiki/Changelog#vs-2022-1710
    add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
    # Windows target architecture macros are set above from the detected compiler target.
    # Non-english windows system may use other encodings other than utf-8 (e.g. Chinese use GBK).
    add_compile_options("/utf-8")
    # Large test objects (e.g. test/transaction/checkpoint_test.cpp) exceed the
    # default 64k section limit; /bigobj raises it (fatal error C1128 otherwise).
    add_compile_options("/bigobj")
    # Enables support for custom hardware exception handling
    add_compile_options("/EHa")
    # Reduces the size of the static library by roughly 1/2
    add_compile_options("/Zc:inline")
    # Disable type conversion warnings
    add_compile_options(/wd4244 /wd4267)
    # Remove the default to avoid warnings
    STRING(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    STRING(REPLACE "/EHs" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # Store all libraries and binaries in the same directory so that riag_shared.dll is found at runtime
    set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/src")
    set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/src")
    # This is a workaround for regex stackoverflow issue on windows in gtest.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:8388608")

    string(REGEX REPLACE "/W[3|4]" "/w" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options($<$<CONFIG:Release>:/W0>)
else()
    add_compile_options(-Wall -Wextra)
    # Disable warnings for unknown pragmas, which is used by several third-party libraries
    add_compile_options(-Wno-unknown-pragmas)
endif()

if(${BUILD_WASM})
    if(NOT __SINGLE_THREADED__)
        add_compile_options(-pthread)
        add_link_options(-pthread)
        add_link_options(-sPTHREAD_POOL_SIZE=8)
        add_link_options(-sPTHREAD_POOL_SIZE_STRICT=0)
    endif()
    add_compile_options(-s DISABLE_EXCEPTION_CATCHING=0)
    add_link_options(-sSTACK_SIZE=4MB)
    add_link_options(-sASSERTIONS=1)
    add_link_options(-lembind)
    add_link_options(-sWASM_BIGINT)

    if(BUILD_TESTS OR BUILD_EXTENSION_TESTS)
        # Tests need full host filesystem access via legacy NODERAWFS.
        # WASMFS's NODERAWFS backend does not support recursive_directory_iterator
        # which is required for test discovery and test file scanning.
        add_link_options(-sINITIAL_MEMORY=3892MB)
        add_link_options(-sNODERAWFS=1)
    elseif(WASM_NODEFS)
        add_link_options(-sNODERAWFS=1)
        add_link_options(-sALLOW_MEMORY_GROWTH=1)
        add_link_options(-sMODULARIZE=1)
        add_link_options(-sEXPORTED_RUNTIME_METHODS=FS,wasmMemory)
        add_link_options(-sEXPORT_NAME=riag)
        add_link_options(-sMAXIMUM_MEMORY=4GB)
    else()
        add_link_options(-sWASMFS)
        add_link_options(-lwasmfs)
        add_link_options(-sSINGLE_FILE=1)
        add_link_options(-sALLOW_MEMORY_GROWTH=1)
        add_link_options(-sMODULARIZE=1)
        add_link_options(-sEXPORT_NAME=riag)
        add_link_options(-sMAXIMUM_MEMORY=4GB)
        if(NOT __SINGLE_THREADED__)
            # OPFS requires pthreads — the Emscripten OPFS JS backend
            # (-lopfs.js) internally uses _emscripten_proxy_finish which is
            # only defined when pthreads are enabled.  Linking -lopfs.js
            # without -pthread causes wasmfsOPFSProxyFinish to compile as a
            # no-op, leading to UNREACHABLE at opfs_backend.cpp:292.
            add_link_options(-lopfs.js)
            add_link_options(-sEXPORTED_RUNTIME_METHODS=FS,wasmMemory,allocateUTF8)
            add_link_options("-sEXPORTED_FUNCTIONS=_free,_wasmfs_create_opfs_backend,_wasmfs_create_directory")
        else()
            add_link_options(-sEXPORTED_RUNTIME_METHODS=FS,wasmMemory)
        endif()
    endif()
    set(__WASM__ TRUE)
    add_compile_options(-fexceptions)
    add_link_options(-s DISABLE_EXCEPTION_CATCHING=0)
    add_link_options(-fexceptions)
    add_compile_definitions(__WASM__)
    set(ENABLE_MALLOC_BUFFER_MANAGER ON)
endif()

if(${BUILD_SWIFT})
    add_compile_definitions(__SWIFT__)
    set(ENABLE_MALLOC_BUFFER_MANAGER ON)
endif()

if (${ENABLE_MALLOC_BUFFER_MANAGER})
    add_compile_definitions(BM_MALLOC)
endif()

if(ANDROID_ABI)
    message(STATUS "Android ABI detected: ${ANDROID_ABI}")
    add_compile_definitions(__ANDROID__)
    set(__ANDROID__ TRUE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-restrict) # no restrict until https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105651 is fixed
endif()

if(${ENABLE_THREAD_SANITIZER} AND (NOT __SINGLE_THREADED__))
    if(MSVC)
        message(FATAL_ERROR "Thread sanitizer is not supported on MSVC")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")
    endif()
endif()
if(${ENABLE_ADDRESS_SANITIZER})
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    endif()
endif()
if(${ENABLE_UBSAN})
    if(MSVC)
        message(FATAL_ERROR "Undefined behavior sanitizer is not supported on MSVC")
    else()
        # Alignment checks are disabled: factorized-table tuples and overflow buffers pack
        # data without padding, so misaligned accesses are pervasive by design and are
        # handled via memcpy where it matters. Append -fno-sanitize=alignment after
        # -fsanitize=undefined so it takes effect regardless of CFLAGS/CXXFLAGS ordering.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize=alignment -fno-omit-frame-pointer")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fno-sanitize=alignment -fno-omit-frame-pointer")
    endif()
endif()

option(ENABLE_MEMORY_SANITIZER "Enable memory sanitizer (clang only; needs an MSan-instrumented libc++)." FALSE)
if(${ENABLE_MEMORY_SANITIZER})
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Memory sanitizer requires clang")
    endif()
    # MSan is all-or-nothing: any translation unit left uninstrumented hands the
    # rest of the program values it cannot reason about, and those surface as
    # false reports. So unlike the sanitizers above, this one instruments C as
    # well as C++ - third_party carries a lot of C - and the toolchain must
    # supply an instrumented libc++ too (see verification/fuzz/Dockerfile).
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer")
endif()

option(ENABLE_FUZZER "Build libFuzzer targets in test/fuzz (requires clang)." FALSE)
if(${ENABLE_FUZZER})
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ENABLE_FUZZER requires clang; libFuzzer exists under neither GCC nor MSVC")
    endif()
    # The engine gets the coverage instrumentation, not the driver: only the
    # executables in test/fuzz link -fsanitize=fuzzer, which is what supplies
    # main(). Adding the driver here instead would give every target in the
    # tree a second main() and fail to link.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=fuzzer-no-link")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=fuzzer-no-link")
endif()

if(${ENABLE_RUNTIME_CHECKS})
    add_compile_definitions(RUNTIME_CHECKS)
endif()

if (${ENABLE_DESER_DEBUG})
    add_compile_definitions(DESER_DEBUG)
endif()

if(${ENABLE_LTO})
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

option(AUTO_UPDATE_GRAMMAR "Automatically regenerate C++ grammar files on change." TRUE)
option(BUILD_BENCHMARK "Build benchmarks." FALSE)
option(BUILD_WAL_DUMP "Build WAL dump tool." FALSE)
option(BUILD_EXTENSIONS "Semicolon-separated list of extensions to build." "")
option(BUILD_EXAMPLES "Build examples." FALSE)
option(BUILD_JAVA "Build Java API." FALSE)
option(BUILD_NODEJS "Build NodeJS API." FALSE)
option(BUILD_PYTHON "Build Python API." FALSE)
option(BUILD_SHELL "Build Interactive Shell" TRUE)
option(BUILD_SINGLE_FILE_HEADER "Build single file header. Requires Python >= 3.9." TRUE)
option(BUILD_RIAG "Build RiaG." TRUE)
option(BUILD_SHARED_RIAG "Build the shared libriag library." TRUE)
option(BUILD_STATIC_RIAG "Build the bundled static libriag archive." TRUE)
option(RIAG_API_USE_PRECOMPILED_LIB "Link language bindings against a precompiled static libriag archive." FALSE)
set(RIAG_API_PRECOMPILED_LIB_PATH "" CACHE FILEPATH "Path to the precompiled static libriag archive used by language bindings.")
option(ENABLE_BACKTRACES "Enable backtrace printing for fatal signals and termination" FALSE)
option(PREFER_SYSTEM_DEPS "Only download certain deps if not found on the system" TRUE)

if(BUILD_WASM)
    set(BUILD_SHARED_RIAG FALSE CACHE BOOL "Build the shared libriag library." FORCE)
    set(BUILD_STATIC_RIAG TRUE CACHE BOOL "Build the bundled static libriag archive." FORCE)
endif()

if(RIAG_API_USE_PRECOMPILED_LIB)
    set(BUILD_RIAG FALSE CACHE BOOL "Build RiaG." FORCE)
endif()

option(BUILD_LCOV "Build coverage report." FALSE)
if(${BUILD_LCOV})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if (ENABLE_BACKTRACES)
    set(DOWNLOAD_CPPTRACE TRUE)
    if(${PREFER_SYSTEM_DEPS})
        find_package(cpptrace QUIET)
        if(cpptrace_FOUND)
            message(STATUS "Using system cpptrace")
            set(DOWNLOAD_CPPTRACE FALSE)
        endif()
    endif()
    if(${DOWNLOAD_CPPTRACE})
        message(STATUS "Fetching cpptrace from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
            cpptrace
            GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
            GIT_TAG        v0.8.3
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(cpptrace)
    endif()
    add_compile_definitions(RIAG_BACKTRACE)
endif()

function(add_riag_test TEST_NAME)
    set(SRCS ${ARGN})
    add_executable(${TEST_NAME} ${SRCS})
    target_link_libraries(${TEST_NAME} PRIVATE test_helper test_runner graph_test)
    if (ENABLE_BACKTRACES)
        target_link_libraries(${TEST_NAME} PRIVATE register_backtrace_signal_handler)
    endif()
    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/test/include)
    include(GoogleTest)

    if (TEST_NAME STREQUAL "e2e_test")
        gtest_discover_tests(${TEST_NAME}
            DISCOVERY_TIMEOUT 600
            DISCOVERY_MODE PRE_TEST
            TEST_PREFIX e2e_test_
        )
    else()
        gtest_discover_tests(${TEST_NAME}
            DISCOVERY_TIMEOUT 600
            DISCOVERY_MODE PRE_TEST
        )
    endif()
endfunction()

function(add_riag_api_test TEST_NAME)
    set(SRCS ${ARGN})
    add_executable(${TEST_NAME} ${SRCS})
    target_link_libraries(${TEST_NAME} PRIVATE api_graph_test api_test_helper)
    if (ENABLE_BACKTRACES)
        target_link_libraries(${TEST_NAME} PRIVATE register_backtrace_signal_handler)
    endif()
    target_include_directories(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/test/include)
    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME}
        DISCOVERY_TIMEOUT 600
        DISCOVERY_MODE PRE_TEST
    )
endfunction()

# Windows doesn't support dynamic lookup, so we have to link extensions against riag.
if (MSVC AND (NOT "${BUILD_EXTENSIONS}" STREQUAL "") AND NOT RIAG_API_USE_PRECOMPILED_LIB)
    set(BUILD_RIAG TRUE)
endif ()

include_directories(third_party/antlr4_cypher/include)
include_directories(third_party/antlr4_runtime/src)
include_directories(third_party/brotli/c/include)
include_directories(third_party/fast_float/include)
include_directories(third_party/mbedtls/include)
include_directories(third_party/parquet)
include_directories(third_party/snappy)
include_directories(third_party/thrift)
include_directories(third_party/miniz)
include_directories(third_party/yyjson/src)
include_directories(third_party/pybind11/include)
include_directories(third_party/pyparse)
include_directories(third_party/re2/include)
include_directories(third_party/alp/include)
if (${BUILD_TESTS} OR ${BUILD_EXTENSION_TESTS})
    include_directories(third_party/spdlog)
elseif (${BUILD_BENCHMARK})
    include_directories(third_party/spdlog)
endif ()
include_directories(third_party/utf8proc/include)
include_directories(third_party/zstd/include)
include_directories(third_party/httplib)
include_directories(third_party/pcg)
include_directories(third_party/lz4)
include_directories(third_party/roaring_bitmap)
include_directories(SYSTEM third_party/simsimd/include)

add_subdirectory(third_party)

add_library(riag_link_deps INTERFACE)
set(RIAG_STATIC_ARCHIVE_LIBRARIES antlr4_cypher antlr4_runtime brotlidec brotlicommon utf8proc re2 fastpfor parquet snappy thrift yyjson zstd miniz mbedtls lz4 roaring_bitmap simsimd)
set(RIAG_LINK_LIBRARIES ${RIAG_STATIC_ARCHIVE_LIBRARIES} fast_float)
if (NOT __SINGLE_THREADED__)
    list(APPEND RIAG_LINK_LIBRARIES Threads::Threads)
endif()
if(NOT WIN32)
    list(PREPEND RIAG_LINK_LIBRARIES dl)
endif()
if ((NOT APPLE AND NOT BSD AND NOT WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang") AND NOT __WASM__ AND NOT __SINGLE_THREADED__)
    list(PREPEND RIAG_LINK_LIBRARIES atomic)
endif()
if (ENABLE_BACKTRACES)
    list(APPEND RIAG_LINK_LIBRARIES cpptrace::cpptrace)
endif()
if (NOT BUILD_WASM)
    find_package(OpenSSL 3 REQUIRED)
    # Pin to OpenSSL 3.x. `find_package(OpenSSL 3 ...)` accepts any version >= 3,
    # but the macOS Homebrew runner now also provides OpenSSL 4, whose deprecated
    # APIs break -Werror builds and whose libssl.4/libcrypto.4 install names break
    # the macOS relocate/delocate packaging scripts, which all assume OpenSSL 3.
    if (DEFINED OPENSSL_VERSION)
        set(_RIAG_OPENSSL_VERSION "${OPENSSL_VERSION}")
    elseif (DEFINED OpenSSL_VERSION)
        set(_RIAG_OPENSSL_VERSION "${OpenSSL_VERSION}")
    else ()
        set(_RIAG_OPENSSL_VERSION "")
    endif ()
    if (_RIAG_OPENSSL_VERSION MATCHES "^([0-9]+)\\.")
        if (NOT CMAKE_MATCH_1 STREQUAL "3")
            message(FATAL_ERROR "RiaGraph requires OpenSSL 3.x but found ${_RIAG_OPENSSL_VERSION}. "
                "Point CMake at an OpenSSL 3 install, e.g. -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3) on macOS.")
        endif ()
    else ()
        message(WARNING "Could not determine OpenSSL major version; RiaGraph requires OpenSSL 3.x and may fail to build against OpenSSL >= 4.")
    endif ()
    unset(_RIAG_OPENSSL_VERSION)
    # Prefer modern imported targets when available (OpenSSL 3 provides OpenSSL::SSL/OpenSSL::Crypto)
    if (TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
        list(APPEND RIAG_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
    else()
        # Fallback to legacy variables for older CMake/OpenSSL packaging
        list(APPEND RIAG_LINK_LIBRARIES ${OPENSSL_LIBRARIES})
    endif()
endif()
target_link_libraries(riag_link_deps INTERFACE ${RIAG_LINK_LIBRARIES})
unset(RIAG_LINK_LIBRARIES)

add_definitions(-DRIAG_ROOT_DIRECTORY="${PROJECT_SOURCE_DIR}")
add_definitions(-DRIAG_CMAKE_VERSION="${CMAKE_PROJECT_VERSION}")
add_definitions(-DRIAG_EXTENSION_VERSION="0.20.0")

if(BUILD_RIAG OR RIAG_API_USE_PRECOMPILED_LIB)
include_directories(
    src/include
    ${CMAKE_CURRENT_BINARY_DIR}/src/include
)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extension/CMakeLists.txt")
    add_subdirectory(extension)
endif ()

if(BUILD_RIAG)

add_subdirectory(src)

# Link extensions which require static linking.
foreach(ext IN LISTS STATICALLY_LINKED_EXTENSIONS)
    if (${BUILD_EXTENSION_TESTS})
        add_compile_definitions(__STATIC_LINK_EXTENSION_TEST__)
    endif ()
    if(TARGET riag)
        target_link_libraries(riag PRIVATE "riag_${ext}_static_extension")
    endif()
    if(TARGET riag_shared)
        target_link_libraries(riag_shared PRIVATE "riag_${ext}_static_extension")
    endif()
endforeach()

# Emscripten does not produce a useful native shared lib here; the wasm package
# and tests link everything into final JS/wasm artifacts. Keep the legacy
# riag_shared target name available for existing consumers, but make it resolve
# to the static wasm library after real library setup and static extension
# linking are complete.
if(EMSCRIPTEN AND TARGET riag AND NOT TARGET riag_shared)
    add_library(riag_shared ALIAS riag)
endif()

if (${BUILD_TESTS} OR ${BUILD_EXTENSION_TESTS})
    add_subdirectory(test)
elseif (${BUILD_BENCHMARK})
    add_subdirectory(test/test_helper)
endif ()

if(${ENABLE_FUZZER})
    # Deliberately not inside test/CMakeLists.txt. That directory pulls in gtest
    # and the whole e2e suite, none of which the fuzz targets need, and it is
    # gated on BUILD_TESTS - so a fuzz build would have to build the test suite
    # under a sanitizer just to reach two executables.
    add_subdirectory(test/fuzz)
endif()
endif ()

if(BUILD_RIAG OR BUILD_SHELL OR BUILD_JAVA OR BUILD_NODEJS OR BUILD_PYTHON OR BUILD_BENCHMARK OR BUILD_WAL_DUMP OR BUILD_WASM)
    add_subdirectory(tools)
endif ()

if (${BUILD_EXAMPLES})
    add_subdirectory(examples/c)
    add_subdirectory(examples/cpp)
endif()
