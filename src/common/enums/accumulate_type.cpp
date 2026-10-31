#include "common/enums/accumulate_type.h"

#include "common/assert.h"

namespace riag {
namespace common {

std::string AccumulateTypeUtil::toString(AccumulateType type) {
    switch (type) {
    case AccumulateType::REGULAR: {
        return "REGULAR";
    }
    case AccumulateType::OPTIONAL_: {
        return "OPTIONAL";
    }
    }
    UNREACHABLE_CODE;
}

} // namespace common
} // namespace riag
