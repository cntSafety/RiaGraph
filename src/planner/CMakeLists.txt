add_subdirectory(join_order)
add_subdirectory(operator)
add_subdirectory(plan)

add_library(riag_planner
        OBJECT
        join_order_enumerator_context.cpp
        planner.cpp
        query_planner.cpp
        subplans_table.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:riag_planner>
        PARENT_SCOPE)
