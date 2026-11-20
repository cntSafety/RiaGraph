#pragma once

#include "namespace_selector.h"
#include "statement.h"

namespace riag::parser {

enum class NamespaceAction : uint8_t {
    CREATE,
    DROP,
    USE,
    TABLE_ADD,
    TABLE_REMOVE,
    TABLE_SET,
    RENAME
};

class NamespaceStatement final : public Statement {
public:
    explicit NamespaceStatement(NamespaceAction action)
        : Statement{common::StatementType::NAMESPACE}, action{action} {}

    NamespaceAction action;
    std::string name;
    // RENAME only: the namespace's new name.
    std::string newName;
    bool ifExists = false;
    NamespaceSelector selector;
};

} // namespace riag::parser
