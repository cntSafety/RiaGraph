#pragma once

#include "catalog/catalog.h"

namespace riag {
namespace extension {

class RIAG_API CatalogExtension : public catalog::Catalog {
public:
    CatalogExtension() : Catalog() {}

    virtual void init() = 0;

    void invalidateCache();
};

} // namespace extension
} // namespace riag
