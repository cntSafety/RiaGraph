add_subdirectory(map)
add_subdirectory(operator)
add_subdirectory(result)

add_library(riag_processor
        OBJECT
        warning_context.cpp
        physical_plan_util.cpp
        processor.cpp
        processor_task.cpp
        partition_routing.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:riag_processor>
        PARENT_SCOPE)
