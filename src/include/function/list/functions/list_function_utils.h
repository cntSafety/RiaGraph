#pragma once

#include "function/function.h"

namespace riag {
namespace function {

enum class ListOp { Append, Prepend, Concat };

template<ListOp op>
struct ListTypeResolver;

template<>
struct ListTypeResolver<ListOp::Append> {
    static void anyEmpty(std::vector<common::LogicalType>& types, common::LogicalType& targetType);
    static void bothNull(std::vector<common::LogicalType>& types, common::LogicalType& targetType);
    static void leftNull(std::vector<common::LogicalType>& types, common::LogicalType& targetType);
    static void rightNull(std::vector<common::LogicalType>& types, common::LogicalType& targetType);
    static void finalResolver(std::vector<common::LogicalType>& types,
        common::LogicalType& targetType);
};

template<>
struct ListTypeResolver<ListOp::Prepend>
    : ListTypeResolver<ListOp::Append> { /*Prepend empty list resolution follows the same logic as
                                            the Append operation*/
};

template<>
struct ListTypeResolver<ListOp::Concat> {
    static void leftEmpty(std::vector<common::LogicalType>& types, common::LogicalType& targetType);
    static void rightEmpty(std::vector<common::LogicalType>& types,
        common::LogicalType& targetType);
    static void bothNull(std::vector<common::LogicalType>& types, common::LogicalType& targetType);
    static void finalResolver(std::vector<common::LogicalType>& types,
        common::LogicalType& targetType);
};

struct ListFunctionUtils {
public:
    using type_resolver = std::function<void(std::vector<common::LogicalType>& types,
        common::LogicalType& targetType)>;

    static void resolveEmptyList(const ScalarBindFuncInput& input,
        std::vector<common::LogicalType>& types, type_resolver bothEmpty, type_resolver leftEmpty,
        type_resolver rightEmpty, type_resolver finalEmptyListResolver);

    static void resolveNulls(std::vector<common::LogicalType>& types, type_resolver bothNull,
        type_resolver leftNull, type_resolver rightNull, type_resolver finalNullParamResolver);

    static void resolveTypes(const ScalarBindFuncInput& input,
        std::vector<common::LogicalType>& types, type_resolver bothEmpty, type_resolver leftEmpty,
        type_resolver rightEmpty, type_resolver finalEmptyListResolver, type_resolver bothNull,
        type_resolver leftNull, type_resolver rightNull, type_resolver finalNullParamResolver);

    // Two lists of maps (or a list of maps and a map) whose keys differ in order or in set get
    // one element type, combined by field name (union of the keys), as in a list literal
    // (cntSafety/RiaGraph#193). types[1] is an element if rightIsElement, else a list. Any
    // other pair of types is left alone.
    static void combineMapElementTypes(std::vector<common::LogicalType>& types,
        bool rightIsElement);
};

} // namespace function
} // namespace riag
