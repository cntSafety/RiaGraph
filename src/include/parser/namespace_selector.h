#pragma once

#include <string>
#include <vector>

namespace riag::parser {

// Presence is represented by optional<NamespaceSelector> on the owning AST node.
// `all` distinguishes explicit ALL NAMESPACES from a finite identifier list.
// The GLOBAL token is recorded separately so quoted identifiers remain identifiers.
struct NamespaceSelector {
    bool all = false;
    bool global = false;
    std::vector<std::string> names;
};

struct NamespaceMove {
    std::string variable;
    NamespaceSelector destination;
};

} // namespace riag::parser
