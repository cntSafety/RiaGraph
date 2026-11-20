// Compile once as a dlopen module and once as its public-API host. This checks
// cross-library ownership, callbacks, RTTI, and exceptions after ELF function binding.
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "common/exception/binder.h"
#include "main/connection.h"
#include "main/database.h"
#include "processor/result/flat_tuple.h"

#ifndef NAMESPACE_LINKAGE_PLUGIN
#include <dlfcn.h>
#endif

namespace riag::testing {
class Callback {
public:
    virtual ~Callback() = default;
    virtual int64_t value(bool fail) const = 0;
};

class HostCallback final : public Callback {
public:
    int64_t value(bool fail) const override {
        if (fail) {
            throw common::BinderException("host callback");
        }
        return 42;
    }
};
} // namespace riag::testing

using namespace riag;

#ifdef NAMESPACE_LINKAGE_PLUGIN
extern "C" common::Value* namespaceLinkageQuery(main::Database* database,
    testing::Callback* callback) {
    if (typeid(*callback) != typeid(testing::HostCallback) ||
        !dynamic_cast<testing::HostCallback*>(callback)) {
        throw std::runtime_error("Cross-module callback RTTI mismatch.");
    }
    try {
        callback->value(true);
        throw std::runtime_error("Host callback did not throw.");
    } catch (const common::BinderException& error) {
        if (std::string(error.what()).find("host callback") == std::string::npos) {
            throw;
        }
    }
    main::Connection connection(database);
    auto result = connection.query("MATCH (n:P {id:1}) SET n.value=43 RETURN n.value");
    if (!result->isSuccess() || !result->hasNext()) {
        throw std::runtime_error("Plugin query failed.");
    }
    const auto value = result->getNext()->getValue(0)->getValue<int64_t>();
    return new common::Value(value + callback->value(false));
}

extern "C" void namespaceLinkageThrow() {
    throw common::BinderException("plugin exception");
}
#else
int main(int argc, char** argv) try {
    if (argc != 2) {
        throw std::runtime_error("Expected the linkage plugin path.");
    }
    std::unique_ptr<void, int (*)(void*)> module(dlopen(argv[1], RTLD_NOW | RTLD_LOCAL), dlclose);
    if (!module) {
        throw std::runtime_error(dlerror());
    }
    const auto query = reinterpret_cast<common::Value* (*)(main::Database*, testing::Callback*)>(
        dlsym(module.get(), "namespaceLinkageQuery"));
    const auto throwException =
        reinterpret_cast<void (*)()>(dlsym(module.get(), "namespaceLinkageThrow"));
    if (!query || !throwException) {
        throw std::runtime_error("Missing linkage probe entry points.");
    }
    main::SystemConfig config;
    config.bufferPoolSize = 64 * 1024 * 1024;
    config.maxNumThreads = 1;
    main::Database database("", config);
    main::Connection connection(&database);
    const auto run = [&](const std::string& statement) {
        auto result = connection.query(statement);
        if (!result->isSuccess()) {
            throw std::runtime_error(result->getErrorMessage());
        }
        return result;
    };
    run("CREATE NODE TABLE P(id INT64 PRIMARY KEY, value INT64)");
    run("CREATE (:P {id:1,value:42})");
    testing::HostCallback callback;
    std::unique_ptr<common::Value> value(query(&database, &callback));
    if (value->getValue<int64_t>() != 85) {
        throw std::runtime_error("Cross-module Value/callback result mismatch.");
    }
    auto result = run("MATCH (n:P) RETURN n.value");
    if (result->getNext()->getValue(0)->getValue<int64_t>() != 43) {
        throw std::runtime_error("Plugin write was not visible to its host.");
    }
    bool caught = false;
    try {
        throwException();
    } catch (const common::BinderException& error) {
        caught = std::string(error.what()).find("plugin exception") != std::string::npos;
    }
    if (!caught) {
        throw std::runtime_error("Cross-module engine exception mismatch.");
    }
    std::cout << "Shared API state, Value ownership, callback RTTI, and exceptions pass.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
#endif
