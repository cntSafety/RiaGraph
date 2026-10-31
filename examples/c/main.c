#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "riag.h"

int main() {
    riag_database db;
    riag_connection conn;
    riag_database_init("" /* fill db path */, riag_default_system_config(), &db);
    riag_connection_init(&db, &conn);

    // Create schema.
    riag_query_result result;
    riag_connection_query(
        &conn, "CREATE NODE TABLE Person(name STRING, age INT64, PRIMARY KEY(name));", &result);
    riag_query_result_destroy(&result);
    // Create nodes.
    riag_connection_query(&conn, "CREATE (:Person {name: 'Alice', age: 25});", &result);
    riag_query_result_destroy(&result);
    riag_connection_query(&conn, "CREATE (:Person {name: 'Bob', age: 30});", &result);
    riag_query_result_destroy(&result);

    // Execute a simple query.
    riag_connection_query(&conn, "MATCH (a:Person) RETURN a.name AS NAME, a.age AS AGE;", &result);

    // Fetch each value.
    riag_flat_tuple tuple;
    riag_value value;
    while (riag_query_result_has_next(&result)) {
        riag_query_result_get_next(&result, &tuple);

        riag_flat_tuple_get_value(&tuple, 0, &value);
        char* name;
        riag_value_get_string(&value, &name);

        riag_flat_tuple_get_value(&tuple, 1, &value);
        int64_t age;
        riag_value_get_int64(&value, &age);

        printf("name: %s, age: %" PRIi64 " \n", name, age);
        riag_destroy_string(name);
    }
    riag_value_destroy(&value);
    riag_flat_tuple_destroy(&tuple);

    // Print query result.
    char* result_string = riag_query_result_to_string(&result);
    printf("%s", result_string);
    riag_destroy_string(result_string);

    riag_query_result_destroy(&result);
    riag_connection_destroy(&conn);
    riag_database_destroy(&db);
    return 0;
}
