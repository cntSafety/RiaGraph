// Diagnostic only: compile with GCC -fno-access-control -Wno-invalid-offsetof.
// Compare common hot members against the frozen baseline without constructing data.
#include <cstddef>
#include <iostream>

#include "main/client_context.h"
#include "main/prepared_statement.h"
#include "storage/local_storage/local_node_table.h"
#include "storage/table/node_table.h"
#include "storage/table/rel_table.h"
#include "transaction/transaction.h"

#define OFFSET(type, member) std::cout << #type "." #member << '=' << offsetof(type, member) << '\n'

int main() {
    using namespace riag::main;
    using namespace riag::storage;
    using namespace riag::transaction;
    OFFSET(ClientContext, activeQuery);
    OFFSET(ClientContext, transactionContext);
    OFFSET(ClientContext, localDatabase);
    OFFSET(CachedPreparedStatement, physicalPlanPoolMutex);
    OFFSET(CachedPreparedStatement, physicalPlanPool);
    OFFSET(Table, tableID);
    OFFSET(Table, lastCheckpointedEpoch);
    OFFSET(NodeTable, schemaMtx);
    OFFSET(NodeTable, nodeGroups);
    OFFSET(NodeTable, indexes);
    OFFSET(RelTable, nextRelOffset);
    OFFSET(RelTable, directedRelData);
    OFFSET(LocalNodeTable, hashIndex);
    OFFSET(LocalNodeTable, nodeGroups);
    OFFSET(Transaction, localStorage);
    OFFSET(Transaction, changedCatalogsMutex);
}
