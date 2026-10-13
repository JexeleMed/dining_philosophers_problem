// Dining philosophers problem - solution WITHOUT STARVATION
// Monitor with a fair queue (tickets, FIFO among neighbours).
//
// Rules:
//  * A philosopher is in one of the states: THINKING, HUNGRY, EATING.
//  * When it gets hungry, it receives an increasing ticket number.
//  * It may start eating only when:
//      - no neighbour is eating, AND
//      - no hungry neighbour has an earlier ticket.
//  * Both forks are taken atomically (under a single mutex) -> no hold-and-wait,
//    i.e. no deadlock.
//
// Why is there no starvation?
//  The philosopher with the smallest ticket among all hungry ones is not blocked
//  by any hungry neighbour, and eating neighbours will eventually finish. So there
//  is always someone who is "next in line". Tickets keep growing, so every hungry
//  philosopher gets its turn after a finite number of meals eaten by others.

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "solutions.h"

namespace fair {

constexpr int N = 5;
constexpr int MEALS = 20;

using Clock = std::chrono::steady_clock;

class Table {
public:
    void pickup(int i) {
        std::unique_lock<std::mutex> lk(m_);
        state_[i] = HUNGRY;
        ticket_[i] = next_ticket_++;
        cv_.wait(lk, [&] { return can_eat(i); });
        state_[i] = EATING;
    }

    void putdown(int i) {
        {
            std::lock_guard<std::mutex> lk(m_);
            state_[i] = THINKING;
        }
        cv_.notify_all();  // neighbours (and others) will re-check their conditions
    }

private:
    enum State { THINKING, HUNGRY, EATING };

    bool can_eat(int i) const {
        for (int nb : {(i + N - 1) % N, (i + 1) % N}) {
            if (state_[nb] == EATING) return false;
            if (state_[nb] == HUNGRY && ticket_[nb] < ticket_[i]) return false;
        }
        return true;
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::array<State, N> state_{};          // THINKING by default
    std::array<unsigned long, N> ticket_{};
    unsigned long next_ticket_ = 0;
};

Table table;
std::mutex out_mtx;
std::array<int, N> meals_eaten{};
std::array<double, N> max_wait_ms{};
std::array<double, N> total_wait_ms{};

void random_sleep(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    std::this_thread::sleep_for(std::chrono::milliseconds(d(rng)));
}

void philosopher(int id) {
    std::mt19937 rng(std::random_device{}() + id);

    for (int i = 0; i < MEALS; ++i) {
        // Philosopher 0 is a "glutton" - it barely thinks, to check
        // whether it starves its neighbours.
        if (id == 0) random_sleep(rng, 0, 5);
        else         random_sleep(rng, 10, 40);

        auto t0 = Clock::now();
        table.pickup(id);
        double wait = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        {
            std::lock_guard<std::mutex> lk(out_mtx);
            std::cout << "Philosopher " << id << " is eating (meal " << i + 1 << ")" << std::endl;
        }
        random_sleep(rng, 10, 30);

        table.putdown(id);

        // only the thread with this id writes the stats -> no race
        ++meals_eaten[id];
        total_wait_ms[id] += wait;
        max_wait_ms[id] = std::max(max_wait_ms[id], wait);
    }
}

int run() {
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) threads.emplace_back(philosopher, i);
    for (auto& t : threads) t.join();

    std::cout << "\n=== Statistics ===\n";
    for (int i = 0; i < N; ++i) {
        std::cout << "Philosopher " << i << ": meals=" << meals_eaten[i]
                  << ", avg wait=" << total_wait_ms[i] / meals_eaten[i]
                  << " ms, max=" << max_wait_ms[i] << " ms\n";
    }
    std::cout << "Every philosopher ate all their meals - nobody was starved.\n";
    return 0;
}

}  // namespace fair
