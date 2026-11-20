#pragma once

#include <optional>
#include <string>

#include "parser/expression/parsed_expression.h"
#include "parser/namespace_selector.h"

namespace riag {
namespace parser {

class NodePattern {
public:
    NodePattern(std::string name, std::vector<std::string> tableNames,
        std::vector<s_parsed_expr_pair> propertyKeyVals)
        : variableName{std::move(name)}, tableNames{std::move(tableNames)},
          propertyKeyVals{std::move(propertyKeyVals)} {}
    DELETE_COPY_DEFAULT_MOVE(NodePattern);

    virtual ~NodePattern() = default;

    inline std::string getVariableName() const { return variableName; }

    inline std::vector<std::string> getTableNames() const { return tableNames; }

    inline const std::vector<s_parsed_expr_pair>& getPropertyKeyVals() const {
        return propertyKeyVals;
    }

    void setNamespaceSelector(NamespaceSelector selector) {
        namespaceSelector = std::move(selector);
    }
    const std::optional<NamespaceSelector>& getNamespaceSelector() const {
        return namespaceSelector;
    }

protected:
    std::string variableName;
    std::vector<std::string> tableNames;
    std::vector<s_parsed_expr_pair> propertyKeyVals;
    std::optional<NamespaceSelector> namespaceSelector;
};

} // namespace parser
} // namespace riag
