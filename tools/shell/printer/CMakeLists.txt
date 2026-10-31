include_directories(
    ${PROJECT_SOURCE_DIR}/third_party/yyjson/src
)

add_library(riag_shell_printer
        OBJECT
        json_printer.cpp
        printer_factory.cpp
)
if(NOT MSVC OR TARGET riag)
    target_sources(riag_shell_printer PRIVATE ${PROJECT_SOURCE_DIR}/src/common/types/json_type.cpp)
endif()
if(WIN32 AND TARGET riag)
    target_compile_definitions(riag_shell_printer PUBLIC RIAG_STATIC_DEFINE)
endif()

set(RIAG_SHELL_OBJECT_FILES
        ${RIAG_SHELL_OBJECT_FILES} $<TARGET_OBJECTS:riag_shell_printer>
        PARENT_SCOPE)
