// Dining philosophers problem - solution 2: WAITER (counting semaphore N-1)
//
// At most N-1 philosophers may sit at the table (try to eat) at the same time.
// Since there are N forks, at least one philosopher will always grab both
// forks, eat and release them - so a deadlock cannot occur.
//
// The semaphore is implemented by hand (mutex + condition_variable) so the code
// already works on C++11/14/17. On C++20 you could use std::counting_semaphore.

#include <array>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "solutions.h"

namespace waiter {

constexpr int N = 5;
constexpr int MEALS = 5;

class Semaphore {
public:
    explicit Semaphore(int count) : count_(count) {}

    void acquire() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return count_ > 0; });
        --count_;
    }

    void release() {
        {
            std::lock_guard<std::mutex> lk(m_);
            ++count_;
        }
        cv_.notify_one();
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    int count_;
};

std::array<std::mutex, N> forks;
Semaphore waiter_sem(N - 1);  // the "waiter" lets in at most N-1 people
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

    for (int i = 0; i < MEALS; ++i) {
        log(id, "is thinking");
        random_sleep(rng);

        waiter_sem.acquire();  // ask the waiter for permission
        {
            std::lock_guard<std::mutex> l1(forks[left]);
            std::lock_guard<std::mutex> l2(forks[right]);

            log(id, "is eating");
            random_sleep(rng);
        }  // forks put down
        waiter_sem.release();  // give up the seat at the table
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

}  // namespace waiter
