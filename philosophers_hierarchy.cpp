// Dining philosophers problem - solution 1: RESOURCE HIERARCHY
//
// Every fork has a number. A philosopher always picks up the LOWER-numbered
// fork first, then the higher-numbered one. This makes a circular wait
// impossible (one of the necessary conditions for a deadlock), so a deadlock
// cannot occur.

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "solutions.h"

namespace hierarchy {

constexpr int N = 5;      // number of philosophers and forks
constexpr int MEALS = 5;  // how many times each philosopher eats

std::array<std::mutex, N> forks;
std::mutex out_mtx;

void log(int id, const char* msg) {
    std::lock_guard<std::mutex> lk(out_mtx);
    std::cout << "Philosopher " << id << " " << msg << std::endl;
}

void random_sleep(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(50, 150);
    std::this_thread::sleep_for(std::chrono::milliseconds(d(rng)));
}

void philosopher(int id) {
    std::mt19937 rng(std::random_device{}() + id);

    const int left = id;
    const int right = (id + 1) % N;
    // Key element: fixed order of acquiring forks
    const int first = std::min(left, right);
    const int second = std::max(left, right);

    for (int i = 0; i < MEALS; ++i) {
        log(id, "is thinking");
        random_sleep(rng);

        std::lock_guard<std::mutex> lock1(forks[first]);
        std::lock_guard<std::mutex> lock2(forks[second]);

        log(id, "is eating");
        random_sleep(rng);
        // forks released automatically (RAII) in reverse order
    }
    log(id, "is done (full)");
}

int run() {
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) threads.emplace_back(philosopher, i);
    for (auto& t : threads) t.join();
    std::cout << "All philosophers have eaten. No deadlock." << std::endl;
    return 0;
}

}  // namespace hierarchy
