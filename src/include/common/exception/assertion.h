#pragma once

#include "common/api.h"
#include "exception.h"

namespace riag {
namespace common {

// Raised by a Cypher ASSERT clause whose condition is FALSE or NULL. Like every error raised while
// a statement runs, it rolls back the transaction. Not to be confused with ASSERT/DASSERT in
// common/assert.h, which guard internal invariants and raise InternalException.
class RIAG_API AssertionException : public Exception {
public:
    explicit AssertionException(const std::string& msg) : Exception("Assertion failed: " + msg) {};
};

} // namespace common
} // namespace riag
