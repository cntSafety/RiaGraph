#pragma once

#include "binder/bound_statement.h"
#include "parser/namespace_statement.h"

namespace riag::binder {

class BoundNamespaceStatement final : public BoundStatement {
public:
    explicit BoundNamespaceStatement(const parser::NamespaceStatement& statement)
        : BoundStatement{common::StatementType::NAMESPACE,
              BoundStatementResult::createSingleStringColumnResult()},
          action{statement.action}, name{statement.name}, newName{statement.newName},
          ifExists{statement.ifExists}, selector{statement.selector} {}

    parser::NamespaceAction action;
    std::string name;
    std::string newName;
    bool ifExists;
    parser::NamespaceSelector selector;
};

} // namespace riag::binder
