include_directories(
        include
        ${PROJECT_SOURCE_DIR}/third_party/taywee_args/include
        ${PROJECT_SOURCE_DIR}/third_party/yyjson/src
        ${PROJECT_SOURCE_DIR}/extension/json/src/include
)

add_subdirectory(printer)

add_executable(riag_shell
        ${RIAG_SHELL_OBJECT_FILES}
        embedded_shell.cpp
        linenoise.cpp
        shell_runner.cpp)

# Static link: avoid LNK4217 (locally defined symbol imported) when linking riag.lib
if(WIN32 AND TARGET riag)
    target_compile_definitions(riag_shell PRIVATE RIAG_STATIC_DEFINE)
endif()

if (NOT MSVC)
    # On Windows, rename the executable to riag.exe will cause an error
    # "multiple rules generate src/riag.lib", so we do not set the output
    # name for MSVC.
    set_target_properties(riag_shell PROPERTIES OUTPUT_NAME riag)
endif ()


if(TARGET riag)
    target_link_libraries(riag_shell riag)
else()
    target_link_libraries(riag_shell riag_shared)
endif()
if (ENABLE_BACKTRACES)
    target_link_libraries(riag_shell register_backtrace_signal_handler)
endif ()
if (MSVC)
    target_link_libraries(riag_shell ws2_32)
endif ()

install(TARGETS riag_shell)
