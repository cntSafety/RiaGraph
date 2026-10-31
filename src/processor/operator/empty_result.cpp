#include "processor/operator/empty_result.h"

namespace riag {
namespace processor {

bool EmptyResult::getNextTuplesInternal(ExecutionContext*) {
    return false;
}

} // namespace processor
} // namespace riag
