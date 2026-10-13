// Dining philosophers problem - NAIVE solution, demonstrates a DEADLOCK
//
// Every philosopher picks up the LEFT fork, then the RIGHT one. After taking
// the left fork it pauses for a random moment (--pause), which makes it likely
// that sooner or later all philosophers hold their left fork at the same time. Then
// each one waits for the fork held by its right neighbour - a circular wait,
// nobody can continue. Until that moment the table works normally.
//
// All four Coffman conditions hold here:
//   1. mutual exclusion  - a fork is held by one philosopher at a time,
//   2. hold and wait     - the left fork is kept while waiting for the right one,
//   3. no preemption     - nobody can take a fork away from its owner,
//   4. circular wait     - 0 waits for 1, 1 for 2, ..., N-1 for 0.
// The other solutions break one of them: hierarchy breaks (4), the waiter
// limits the number of philosophers that can form the cycle to N-1, and the
// fair monitor takes both forks at once, which breaks (2).
//
// Forks are std::timed_mutex and waiting uses try_lock_for in a loop, so once
// the watchdog detects the deadlock, the threads can be stopped cleanly.

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#include "solutions.h"

namespace deadlock {

namespace {

constexpr auto POLL = std::chrono::milliseconds(10);
constexpr auto CONFIRM = std::chrono::milliseconds(200);     // all must hold left this long

enum State { THINKING, HUNGRY, HAS_LEFT, EATING };

// Waits for the fork; false = the run was stopped in the meantime.
bool take(std::timed_mutex& fork, const Experiment& ex) {
    while (!fork.try_lock_for(POLL)) {
        if (ex.stopped()) return false;
    }
    return true;
}

void philosopher(int id, Experiment& ex, std::vector<std::timed_mutex>& forks,
                 std::vector<std::atomic<int>>& state) {
    auto rng = ex.make_rng(id);

    auto& left = forks[id];
    auto& right = forks[(id + 1) % ex.n()];

    while (!ex.stopped()) {
        state[id] = THINKING;
        ex.think(id, rng);
        if (ex.stopped()) break;

        ex.hungry(id);
        state[id] = HUNGRY;
        if (!take(left, ex)) break;
        state[id] = HAS_LEFT;
        ex.log(id, "took the left fork");
        std::uniform_int_distribution<int> pause(ex.cfg().pause_min_ms, ex.cfg().pause_max_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(pause(rng)));

        if (!take(right, ex)) {
            left.unlock();
            break;
        }
        if (!ex.start_eating(id)) {
            right.unlock();
            left.unlock();
            break;
        }
        state[id] = EATING;
        ex.eat(id, rng);
        state[id] = THINKING;
        right.unlock();
        left.unlock();
    }
}

}  // namespace

Summary run(const Config& cfg) {
    Experiment ex(cfg, "deadlock");
    std::vector<std::timed_mutex> forks(cfg.n);
    std::vector<std::atomic<int>> state(cfg.n);

    // Watchdog: deadlock = every philosopher holds its left fork and waits for the right one.
    std::optional<Clock::time_point> all_hold_left_since;
    auto watch = [&] {
        for (const auto& s : state) {
            if (s != HAS_LEFT) {
                all_hold_left_since.reset();
                return false;
            }
        }
        auto now = Clock::now();
        if (!all_hold_left_since) all_hold_left_since = now;
        if (now - *all_hold_left_since < CONFIRM) return false;

        ex.mark_deadlock(*all_hold_left_since);
        std::ostringstream os;
        os << "DEADLOCK detected - circular wait:";
        if (cfg.n <= 10) {
            for (int i = 0; i < cfg.n; ++i) {
                int next = (i + 1) % cfg.n;
                os << "\n  philosopher " << i << " holds fork " << i << ", waits for fork " << next
                   << " (held by philosopher " << next << ")";
            }
        } else {
            os << "\n  philosopher i holds fork i and waits for fork i+1, for all i = 0.." << cfg.n - 1;
        }
        ex.note(os.str());
        return true;
    };

    return ex.run([&](int id) { philosopher(id, ex, forks, state); }, watch);
}

}  // namespace deadlock
