#include <memory>

#include "printer/printer.h"

namespace riag {
namespace main {

class PrinterFactory {
public:
    static std::unique_ptr<Printer> getPrinter(PrinterType type);
};

} // namespace main
} // namespace riag
