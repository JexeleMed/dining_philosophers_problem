// Dining philosophers problem - solution 1: RESOURCE HIERARCHY

#include <algorithm>
#include <mutex>
#include <vector>

#include "solutions.h"

namespace hierarchy {

namespace {

void philosopher(int id, Experiment& ex, std::vector<std::mutex>& forks) {
    auto rng = ex.make_rng(id);

    const int left = id;
    const int right = (id + 1) % ex.n();
    // Key element: fixed order of acquiring forks
    const int first = std::min(left, right);
    const int second = std::max(left, right);

    while (!ex.stopped()) {
        ex.think(id, rng);
        if (ex.stopped()) break;

        ex.hungry(id);
        std::lock_guard<std::mutex> lock1(forks[first]);
        std::lock_guard<std::mutex> lock2(forks[second]);

        if (!ex.start_eating(id)) break;
        ex.eat(id, rng);
        // forks released automatically in reverse order
    }
}

}  // namespace

Summary run(const Config& cfg) {
    Experiment ex(cfg, "hierarchy");
    std::vector<std::mutex> forks(cfg.n);
    return ex.run([&](int id) { philosopher(id, ex, forks); });
}

}  // namespace hierarchy
