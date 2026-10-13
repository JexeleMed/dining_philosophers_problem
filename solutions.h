// Entry-point declarations for the three solutions to the dining philosophers
// problem. Each solution lives in its own namespace so that global names
// (N, MEALS, forks, philosopher, ...) don't collide once linked into one program.
#ifndef SOLUTIONS_H
#define SOLUTIONS_H

namespace hierarchy { int run(); }  // solution 1: resource hierarchy
namespace waiter    { int run(); }  // solution 2: waiter (counting semaphore N-1)
namespace fair      { int run(); }  // solution 3: monitor with a fair queue

#endif  // SOLUTIONS_H
