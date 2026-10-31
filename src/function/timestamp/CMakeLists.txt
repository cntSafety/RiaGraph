add_library(riag_timestamp_function
        OBJECT
        time_bucket.cpp
        to_epoch_ms.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:riag_timestamp_function>
        PARENT_SCOPE)
