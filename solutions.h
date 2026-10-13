// Entry-point declarations for the solutions to the dining philosophers
// problem. Each solution lives in its own namespace.
#ifndef SOLUTIONS_H
#define SOLUTIONS_H

#include "common.h"

namespace hierarchy { Summary run(const Config& cfg); }  // solution 1: resource hierarchy
namespace waiter    { Summary run(const Config& cfg); }  // solution 2: waiter (counting semaphore N-1)
namespace fair      { Summary run(const Config& cfg); }  // solution 3: monitor with a fair queue
namespace deadlock  { Summary run(const Config& cfg); }  // naive solution - deadlock demo

#endif  // SOLUTIONS_H
