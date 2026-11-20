#pragma once

#include "parser/namespace_statement.h"
#include "processor/operator/sink.h"

namespace riag::processor {

class NamespaceCommand final : public SimpleSink {
public:
    NamespaceCommand(parser::NamespaceAction action, std::string name, std::string newName,
        bool ifExists, parser::NamespaceSelector selector,
        std::shared_ptr<FactorizedTable> messageTable, physical_op_id id,
        std::unique_ptr<OPPrintInfo> printInfo)
        : SimpleSink{PhysicalOperatorType::NAMESPACE_COMMAND, std::move(messageTable), id,
              std::move(printInfo)},
          action{action}, name{std::move(name)}, newName{std::move(newName)}, ifExists{ifExists},
          selector{std::move(selector)} {}

    void executeInternal(ExecutionContext* context) override;
    std::unique_ptr<PhysicalOperator> copy() override {
        return std::make_unique<NamespaceCommand>(action, name, newName, ifExists, selector,
            messageTable, id, printInfo->copy());
    }

private:
    parser::NamespaceAction action;
    std::string name;
    std::string newName;
    bool ifExists;
    parser::NamespaceSelector selector;
};

} // namespace riag::processor
