#include "function/cast/functions/cast_array.h"

#include "common/exception/conversion.h"
#include "common/type_utils.h"
#include <format>

namespace riag {
namespace function {

bool CastArrayHelper::checkCompatibleNestedTypes(LogicalTypeID sourceTypeID,
    LogicalTypeID targetTypeID) {
    switch (sourceTypeID) {
    case LogicalTypeID::ANY: {
        return true;
    }
    case LogicalTypeID::LIST: {
        return targetTypeID == LogicalTypeID::ARRAY || targetTypeID == LogicalTypeID::LIST;
    }
    case LogicalTypeID::UNION:
    case LogicalTypeID::MAP:
    case LogicalTypeID::STRUCT: {
        return sourceTypeID == targetTypeID || targetTypeID == LogicalTypeID::UNION;
    }
    case LogicalTypeID::ARRAY: {
        return targetTypeID == LogicalTypeID::LIST || targetTypeID == LogicalTypeID::ARRAY;
    }
    default:
        return false;
    }
}

bool CastArrayHelper::isUnionSpecialCast(const LogicalType& srcType, const LogicalType& dstType) {
    if (srcType.getLogicalTypeID() != LogicalTypeID::STRUCT ||
        dstType.getLogicalTypeID() != LogicalTypeID::UNION ||
        !StructType::hasField(srcType, "tag")) {
        return false;
    }
    for (auto& field : StructType::getFields(srcType)) {
        if (!UnionType::hasField(dstType, field.getName()) ||
            UnionType::getFieldType(dstType, field.getName()) != field.getType()) {
            return false;
        }
    }
    return true;
}

bool CastArrayHelper::containsListToArray(const LogicalType& srcType, const LogicalType& dstType) {
    auto srcTypeID = srcType.getLogicalTypeID();
    auto dstTypeID = dstType.getLogicalTypeID();

    if ((srcTypeID == LogicalTypeID::LIST || srcTypeID == LogicalTypeID::ARRAY) &&
        dstTypeID == LogicalTypeID::ARRAY) {
        return true;
    }

    if (!isUnionSpecialCast(srcType, dstType) &&
        (srcTypeID == LogicalTypeID::UNION || dstTypeID == LogicalTypeID::UNION)) {
        return false;
    }

    if (checkCompatibleNestedTypes(srcTypeID, dstTypeID)) {
        switch (srcType.getPhysicalType()) {
        case PhysicalTypeID::LIST: {
            return containsListToArray(ListType::getChildType(srcType),
                ListType::getChildType(dstType));
        }
        case PhysicalTypeID::ARRAY: {
            return containsListToArray(ArrayType::getChildType(srcType),
                ListType::getChildType(dstType));
        }
        case PhysicalTypeID::STRUCT: {
            // STRUCT fields are cast by name (see resolveNestedVector), so look at the target
            // field of the same name. A source field without one is reported by the cast itself.
            for (const auto& srcField : StructType::getFields(srcType)) {
                if (StructType::hasField(dstType, srcField.getName()) &&
                    containsListToArray(srcField.getType(),
                        StructType::getFieldType(dstType, srcField.getName()))) {
                    return true;
                }
            }
        } break;
        default:
            return false;
        }
    }
    return false;
}

void CastArrayHelper::validateListEntry(ValueVector* inputVector, const LogicalType& resultType,
    uint64_t pos) {
    if (inputVector->isNull(pos)) {
        return;
    }
    const auto& inputType = inputVector->dataType;

    switch (resultType.getPhysicalType()) {
    case PhysicalTypeID::ARRAY: {
        if (inputType.getPhysicalType() == PhysicalTypeID::LIST) {
            auto listEntry = inputVector->getValue<list_entry_t>(pos);
            if (listEntry.size != ArrayType::getNumElements(resultType)) {
                throw ConversionException{
                    std::format("Unsupported casting LIST with incorrect list entry to ARRAY. "
                                "Expected: {}, Actual: {}.",
                        ArrayType::getNumElements(resultType),
                        inputVector->getValue<list_entry_t>(pos).size)};
            }
            auto inputChildVector = ListVector::getDataVector(inputVector);
            for (auto i = listEntry.offset; i < listEntry.offset + listEntry.size; i++) {
                validateListEntry(inputChildVector, ArrayType::getChildType(resultType), i);
            }
        } else if (inputType.getPhysicalType() == PhysicalTypeID::ARRAY) {
            if (ArrayType::getNumElements(inputType) != ArrayType::getNumElements(resultType)) {
                throw ConversionException(std::format("Unsupported casting function from {} to {}.",
                    inputType.toString(), resultType.toString()));
            }
            auto listEntry = inputVector->getValue<list_entry_t>(pos);
            auto inputChildVector = ListVector::getDataVector(inputVector);
            for (auto i = listEntry.offset; i < listEntry.offset + listEntry.size; i++) {
                validateListEntry(inputChildVector, ArrayType::getChildType(resultType), i);
            }
        }
    } break;
    case PhysicalTypeID::LIST: {
        if (inputType.getPhysicalType() == PhysicalTypeID::LIST ||
            inputType.getPhysicalType() == PhysicalTypeID::ARRAY) {
            auto listEntry = inputVector->getValue<list_entry_t>(pos);
            auto inputChildVector = ListVector::getDataVector(inputVector);
            for (auto i = listEntry.offset; i < listEntry.offset + listEntry.size; i++) {
                validateListEntry(inputChildVector, ListType::getChildType(resultType), i);
            }
        }
    } break;
    case PhysicalTypeID::STRUCT: {
        if (inputType.getPhysicalType() == PhysicalTypeID::STRUCT) {
            // Fields are matched by name, as in the cast itself.
            auto fieldVectors = StructVector::getFieldVectors(inputVector);
            const auto& inputFields = StructType::getFields(inputType);
            auto structEntry = inputVector->getValue<struct_entry_t>(pos);
            for (auto i = 0u; i < fieldVectors.size(); i++) {
                const auto& name = inputFields[i].getName();
                if (StructType::hasField(resultType, name)) {
                    validateListEntry(fieldVectors[i].get(),
                        StructType::getFieldType(resultType, name), structEntry.pos);
                }
            }
        }
    } break;
    default: {
        return;
    }
    }
}

} // namespace function
} // namespace riag
