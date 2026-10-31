#include "c_api/riag.h"
#include "common/types/types.h"

using namespace riag::common;

namespace riag::common {
struct CAPIHelper {
    static inline LogicalType* createLogicalType(LogicalTypeID typeID,
        std::unique_ptr<ExtraTypeInfo> extraTypeInfo) {
        return new LogicalType(typeID, std::move(extraTypeInfo));
    }
};
} // namespace riag::common

void riag_data_type_create(riag_data_type_id id, riag_logical_type* child_type,
    uint64_t num_elements_in_array, riag_logical_type* out_data_type) {
    uint8_t data_type_id_u8 = id;
    LogicalType* data_type = nullptr;
    auto logicalTypeID = static_cast<LogicalTypeID>(data_type_id_u8);
    if (child_type == nullptr) {
        data_type = new LogicalType(logicalTypeID);
    } else {
        auto child_type_pty = static_cast<LogicalType*>(child_type->_data_type)->copy();
        auto extraTypeInfo =
            num_elements_in_array > 0 ?
                std::make_unique<ArrayTypeInfo>(std::move(child_type_pty), num_elements_in_array) :
                std::make_unique<ListTypeInfo>(std::move(child_type_pty));
        data_type = CAPIHelper::createLogicalType(logicalTypeID, std::move(extraTypeInfo));
    }
    out_data_type->_data_type = data_type;
}

void riag_data_type_clone(riag_logical_type* data_type, riag_logical_type* out_data_type) {
    out_data_type->_data_type =
        new LogicalType(static_cast<LogicalType*>(data_type->_data_type)->copy());
}

void riag_data_type_destroy(riag_logical_type* data_type) {
    if (data_type == nullptr) {
        return;
    }
    if (data_type->_data_type != nullptr) {
        delete static_cast<LogicalType*>(data_type->_data_type);
        data_type->_data_type = nullptr;
    }
}

bool riag_data_type_equals(riag_logical_type* data_type1, riag_logical_type* data_type2) {
    return *static_cast<LogicalType*>(data_type1->_data_type) ==
           *static_cast<LogicalType*>(data_type2->_data_type);
}

riag_data_type_id riag_data_type_get_id(riag_logical_type* data_type) {
    auto data_type_id_u8 =
        static_cast<uint8_t>(static_cast<LogicalType*>(data_type->_data_type)->getLogicalTypeID());
    return static_cast<riag_data_type_id>(data_type_id_u8);
}

riag_state riag_data_type_get_child_type(riag_logical_type* data_type,
    riag_logical_type* out_result) {
    auto* parent_type = static_cast<LogicalType*>(data_type->_data_type);
    try {
        switch (parent_type->getLogicalTypeID()) {
        case LogicalTypeID::ARRAY:
            out_result->_data_type = new LogicalType(ArrayType::getChildType(*parent_type).copy());
            return RiaGSuccess;
        case LogicalTypeID::LIST:
            out_result->_data_type = new LogicalType(ListType::getChildType(*parent_type).copy());
            return RiaGSuccess;
        default:
            return RiaGError;
        }
    } catch (Exception& e) {
        return RiaGError;
    }
}

riag_state riag_data_type_get_num_elements_in_array(riag_logical_type* data_type,
    uint64_t* out_result) {
    auto parent_type = static_cast<LogicalType*>(data_type->_data_type);
    if (parent_type->getLogicalTypeID() != LogicalTypeID::ARRAY) {
        return RiaGError;
    }
    try {
        *out_result = ArrayType::getNumElements(*parent_type);
    } catch (Exception& e) {
        return RiaGError;
    }
    return RiaGSuccess;
}
