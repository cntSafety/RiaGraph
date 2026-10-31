add_library(riag_processor_operator_csv_reader
        OBJECT
        base_csv_reader.cpp
        csv_boundary_scanner.cpp
        driver.cpp
        parallel_csv_reader.cpp
        serial_csv_reader.cpp
        dialect_detection.cpp)

set(ALL_OBJECT_FILES
        ${ALL_OBJECT_FILES} $<TARGET_OBJECTS:riag_processor_operator_csv_reader>
        PARENT_SCOPE)
