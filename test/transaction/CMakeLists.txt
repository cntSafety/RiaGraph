add_riag_test(current_time_test current_time_test.cpp)

# Also tested for coverage in connection_test.cpp
# but full testing requires some private APIs
add_riag_test(transaction_test transaction_test.cpp checkpoint_test.cpp wal_test.cpp)

# Loaded through LOAD EXTENSION from a WAL replay test; resolves core symbols from the
# test process instead of linking riag, which would duplicate core state.
add_library(riag_test_registry_mutating_extension SHARED registry_mutating_extension.cpp)
target_include_directories(riag_test_registry_mutating_extension PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${CMAKE_BINARY_DIR}/src/include)
set_target_properties(riag_test_registry_mutating_extension PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
if (WIN32)
    # Windows has no dynamic symbol lookup, so the extension must link
    # against the core library. Prefer the shared library so the extension
    # shares state with the test process; the static archive would duplicate
    # core state. The static target may not exist (Windows CI disables it
    # with BUILD_STATIC_RIAG=FALSE because the bundled archive exceeds
    # MSVC's COFF size limit), so fall back to it only when shared is off.
    if (TARGET riag_shared)
        target_link_libraries(riag_test_registry_mutating_extension PRIVATE riag_shared)
    elseif (TARGET riag)
        target_link_libraries(riag_test_registry_mutating_extension PRIVATE riag)
    endif()
endif()
if (APPLE)
    set_target_properties(riag_test_registry_mutating_extension PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup")
endif()
add_dependencies(transaction_test riag_test_registry_mutating_extension)
target_compile_definitions(transaction_test PRIVATE
    "RIAG_TEST_REGISTRY_MUTATING_EXTENSION_PATH=\"$<TARGET_FILE:riag_test_registry_mutating_extension>\"")
