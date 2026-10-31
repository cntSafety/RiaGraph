include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Have to pass this down to every subdirectory, which actually adds the files.
# This doesn't affect parent directories.
if(BUILD_SHARED_RIAG)
        add_compile_definitions(RIAG_EXPORTS)
else()
        add_compile_definitions(RIAG_STATIC_DEFINE)
endif()
add_compile_definitions(ANTLR4CPP_STATIC)
add_subdirectory(binder)
add_subdirectory(c_api)
add_subdirectory(catalog)
add_subdirectory(common)
add_subdirectory(expression_evaluator)
add_subdirectory(function)
add_subdirectory(graph)
add_subdirectory(main)
add_subdirectory(optimizer)
add_subdirectory(parser)
add_subdirectory(planner)
add_subdirectory(processor)
add_subdirectory(storage)
add_subdirectory(transaction)
add_subdirectory(extension)

if(BUILD_SHARED_RIAG)
        add_library(riag_shared SHARED ${ALL_OBJECT_FILES})
endif()
if(BUILD_STATIC_RIAG)
        add_library(riag STATIC ${ALL_OBJECT_FILES})
endif()

if(TARGET riag_shared)
        set_target_properties(riag_shared PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )
        if(WIN32)
            set_target_properties(riag_shared PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
        endif()
endif()

if(TARGET riag_shared)
        target_link_libraries(riag_shared PUBLIC riag_link_deps)
endif()
if(TARGET riag)
        target_link_libraries(riag PUBLIC riag_link_deps)
endif()

if(TARGET riag)
        set(RIAG_STATIC_ARCHIVE_PATHS)
        foreach(LIBRARY IN LISTS RIAG_STATIC_ARCHIVE_LIBRARIES)
                list(APPEND RIAG_STATIC_ARCHIVE_PATHS "$<TARGET_FILE:${LIBRARY}>")
        endforeach()
        set(RIAG_STATIC_ARCHIVE_PATH_FILE "${CMAKE_CURRENT_BINARY_DIR}/riag_static_archives_$<CONFIG>.txt")
        file(GENERATE
                OUTPUT "${RIAG_STATIC_ARCHIVE_PATH_FILE}"
                CONTENT "$<JOIN:${RIAG_STATIC_ARCHIVE_PATHS},\n>\n")
        add_custom_command(TARGET riag POST_BUILD
                COMMAND ${CMAKE_COMMAND}
                        "-DOUTPUT=$<TARGET_FILE:riag>"
                        "-DMAIN_LIBRARY=$<TARGET_FILE:riag>"
                        "-DSTATIC_LIBRARIES_FILE=${RIAG_STATIC_ARCHIVE_PATH_FILE}"
                        "-DAR_EXECUTABLE=${CMAKE_AR}"
                        "-DRANLIB_EXECUTABLE=${CMAKE_RANLIB}"
                        -P "${PROJECT_SOURCE_DIR}/cmake/BundleStaticLibrary.cmake"
                VERBATIM)
endif()

set(RIAG_INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include/c_api ${CMAKE_CURRENT_BINARY_DIR}/../src/include)
if(TARGET riag_shared)
        target_include_directories(riag_shared PUBLIC ${RIAG_INCLUDES})
endif()
if(TARGET riag)
        target_include_directories(riag PUBLIC ${RIAG_INCLUDES})
endif()
unset(RIAG_INCLUDES)

if(WIN32 AND TARGET riag)
        # Anything linking against the static library must not use dllimport.
        target_compile_definitions(riag INTERFACE RIAG_STATIC_DEFINE)
endif()

if(EMSCRIPTEN)
        # Emscripten forces SHARED->STATIC, so give it a distinct name to avoid
        # two targets both emitting libriag.a (Ninja rejects duplicate rules).
        if(TARGET riag_shared)
                set_target_properties(riag_shared PROPERTIES OUTPUT_NAME riag_shared)
        endif()
elseif(NOT WIN32 AND TARGET riag_shared)
        set_target_properties(riag_shared PROPERTIES OUTPUT_NAME riag)
endif()

if(TARGET riag AND TARGET riag_shared)
        install(TARGETS riag riag_shared)
elseif(TARGET riag)
        install(TARGETS riag)
else()
        install(TARGETS riag_shared)
endif()

if(${BUILD_SINGLE_FILE_HEADER})
        # Create a command to generate riag.hpp, and then create a target that is
        # always built that depends on it. This allows our generator to detect when
        # exactly to build riag.hpp, while still building the target by default.
        find_package(Python3 3.9...4 REQUIRED)
        if(TARGET riag_shared)
                set(RIAG_SINGLE_FILE_HEADER_DEP riag_shared)
        else()
                set(RIAG_SINGLE_FILE_HEADER_DEP riag)
        endif()
        add_custom_command(
                OUTPUT riag.hpp
                COMMAND
                        ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/collect-single-file-header.py ${CMAKE_CURRENT_BINARY_DIR}/..
                DEPENDS
                        ${PROJECT_SOURCE_DIR}/scripts/collect-single-file-header.py ${RIAG_SINGLE_FILE_HEADER_DEP})
        add_custom_target(single_file_header ALL DEPENDS riag.hpp)
        unset(RIAG_SINGLE_FILE_HEADER_DEP)
endif()

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/c_api/riag.h TYPE INCLUDE)

if(${BUILD_SINGLE_FILE_HEADER})
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/riag.hpp  TYPE INCLUDE)
endif()
