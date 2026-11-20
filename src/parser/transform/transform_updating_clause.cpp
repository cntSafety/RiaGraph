#include "common/assert.h"
#include "parser/query/updating_clause/delete_clause.h"
#include "parser/query/updating_clause/insert_clause.h"
#include "parser/query/updating_clause/merge_clause.h"
#include "parser/query/updating_clause/set_clause.h"
#include "parser/transformer.h"

namespace riag {
namespace parser {

std::unique_ptr<UpdatingClause> Transformer::transformUpdatingClause(
    CypherParser::OC_UpdatingClauseContext& ctx) {
    if (ctx.oC_Create()) {
        return transformCreate(*ctx.oC_Create());
    } else if (ctx.oC_Merge()) {
        return transformMerge(*ctx.oC_Merge());
    } else if (ctx.oC_Set()) {
        return transformSet(*ctx.oC_Set());
    } else {
        DASSERT(ctx.oC_Delete());
        return transformDelete(*ctx.oC_Delete());
    }
}

std::unique_ptr<UpdatingClause> Transformer::transformCreate(CypherParser::OC_CreateContext& ctx) {
    auto result = std::make_unique<InsertClause>(transformPattern(*ctx.oC_Pattern()));
    if (ctx.iC_NamespaceScope()) {
        result->setNamespaceSelector(
            transformNamespaceList(*ctx.iC_NamespaceScope()->iC_NamespaceList()));
    }
    return result;
}

std::unique_ptr<UpdatingClause> Transformer::transformMerge(CypherParser::OC_MergeContext& ctx) {
    auto mergeClause = std::make_unique<MergeClause>(transformPattern(*ctx.oC_Pattern()));
    if (ctx.iC_NamespaceScope()) {
        mergeClause->setNamespaceSelector(
            transformNamespaceList(*ctx.iC_NamespaceScope()->iC_NamespaceList()));
    }
    for (auto& mergeActionCtx : ctx.oC_MergeAction()) {
        if (mergeActionCtx->MATCH()) {
            for (auto& setItemCtx : mergeActionCtx->oC_Set()->oC_SetItem()) {
                if (setItemCtx->iC_NamespaceName()) {
                    mergeClause->addOnMatchNamespaceMove(transformNamespaceMove(*setItemCtx));
                } else {
                    mergeClause->addOnMatchSetItems(transformSetItem(*setItemCtx));
                }
            }
        } else {
            for (auto& setItemCtx : mergeActionCtx->oC_Set()->oC_SetItem()) {
                if (setItemCtx->iC_NamespaceName()) {
                    mergeClause->addOnCreateNamespaceMove(transformNamespaceMove(*setItemCtx));
                } else {
                    mergeClause->addOnCreateSetItems(transformSetItem(*setItemCtx));
                }
            }
        }
    }
    return mergeClause;
}

std::unique_ptr<UpdatingClause> Transformer::transformSet(CypherParser::OC_SetContext& ctx) {
    auto setClause = std::make_unique<SetClause>();
    if (ctx.iC_Properties()) {
        auto child = transformAtom(*ctx.oC_Atom());
        for (auto i = 0u; i < ctx.iC_Properties()->oC_PropertyKeyName().size(); ++i) {
            auto propertyKeyName = createPropertyExpression(
                *ctx.iC_Properties()->oC_PropertyKeyName(i), child->copy());
            auto expression = transformExpression(*ctx.iC_Properties()->oC_Expression(i));
            setClause->addSetItem(make_pair(std::move(propertyKeyName), std::move(expression)));
        }
    } else {
        for (auto& setItem : ctx.oC_SetItem()) {
            if (setItem->iC_NamespaceName()) {
                setClause->addNamespaceMove(transformNamespaceMove(*setItem));
            } else {
                setClause->addSetItem(transformSetItem(*setItem));
            }
        }
    }
    return setClause;
}

parsed_expr_pair Transformer::transformSetItem(CypherParser::OC_SetItemContext& ctx) {
    return make_pair(transformProperty(*ctx.oC_PropertyExpression()),
        transformExpression(*ctx.oC_Expression()));
}

NamespaceMove Transformer::transformNamespaceMove(CypherParser::OC_SetItemContext& ctx) {
    NamespaceSelector destination;
    auto* name = ctx.iC_NamespaceName();
    if (name->GLOBAL()) {
        destination.global = true;
    } else {
        destination.names.push_back(transformSymbolicName(*name->oC_SymbolicName()));
    }
    return {transformVariable(*ctx.oC_Variable()), std::move(destination)};
}

std::unique_ptr<UpdatingClause> Transformer::transformDelete(CypherParser::OC_DeleteContext& ctx) {
    auto deleteClauseType =
        ctx.DETACH() ? common::DeleteNodeType::DETACH_DELETE : common::DeleteNodeType::DELETE;
    auto deleteClause = std::make_unique<DeleteClause>(deleteClauseType);
    for (auto& expression : ctx.oC_Expression()) {
        deleteClause->addExpression(transformExpression(*expression));
    }
    return deleteClause;
}

} // namespace parser
} // namespace riag
