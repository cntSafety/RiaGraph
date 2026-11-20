#pragma once

#include "common/namespace_set.h"
#include "function/function.h"

namespace riag::binder {
class NodeOrRelExpression;
}

namespace riag::function {

struct RewriteFunctionBindInput;

struct NamespaceFunction {
    static constexpr const char* name = "NAMESPACE";
    static function_set getFunctionSet();
    static std::shared_ptr<binder::Expression> rewriteFunc(const RewriteFunctionBindInput& input);
};

std::shared_ptr<binder::Expression> createNamespacePredicate(
    const binder::NodeOrRelExpression& entity, common::NamespaceSet scope,
    main::ClientContext* context, std::string uniqueName, bool assertion = false);
std::optional<common::NamespaceSet> getNamespacePredicateScope(const binder::Expression& predicate,
    const binder::Expression& internalID);
std::optional<double> getNamespacePredicateSelectivity(const binder::Expression& predicate);

} // namespace riag::function
