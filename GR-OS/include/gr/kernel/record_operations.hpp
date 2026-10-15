#pragma once
#include "gr/kernel/operations.hpp"
namespace gr::kernel {
// Explicit bounded-JSON operand contract. Larger snapshot/history interfaces
// retain their separate limits and are not silently encoded as this type.
void install_record_operations(ValueTypeRegistry &, OperationRegistry &);
} // namespace gr::kernel
