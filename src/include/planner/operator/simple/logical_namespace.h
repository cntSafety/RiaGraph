#pragma once

#include "logical_simple.h"
#include "parser/namespace_statement.h"

namespace riag::planner {

class LogicalNamespace final : public LogicalSimple {
public:
    LogicalNamespace(parser::NamespaceAction action, std::string name, std::string newName,
        bool ifExists, parser::NamespaceSelector selector)
        : LogicalSimple{LogicalOperatorType::NAMESPACE_COMMAND}, action{action},
          name{std::move(name)}, newName{std::move(newName)}, ifExists{ifExists},
          selector{std::move(selector)} {}

    parser::NamespaceAction action;
    std::string name;
    std::string newName;
    bool ifExists;
    parser::NamespaceSelector selector;

    std::string getExpressionsForPrinting() const override { return name; }
    std::unique_ptr<LogicalOperator> copy() override {
        return std::make_unique<LogicalNamespace>(action, name, newName, ifExists, selector);
    }
};

} // namespace riag::planner
