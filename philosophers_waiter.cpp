// Dining philosophers problem - solution 2: WAITER (counting semaphore N-1)

#include <condition_variable>
#include <mutex>
#include <vector>

#include "solutions.h"

namespace waiter {

namespace {

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

void philosopher(int id, Experiment& ex, std::vector<std::mutex>& forks, Semaphore& waiter_sem) {
    auto rng = ex.make_rng(id);

    const int left = id;
    const int right = (id + 1) % ex.n();

    while (!ex.stopped()) {
        ex.think(id, rng);
        if (ex.stopped()) break;

        ex.hungry(id);
        waiter_sem.acquire();  // ask the waiter for permission
        bool ate;
        {
            std::lock_guard<std::mutex> l1(forks[left]);
            std::lock_guard<std::mutex> l2(forks[right]);

            ate = ex.start_eating(id);
            if (ate) ex.eat(id, rng);
        }  // forks put down
        waiter_sem.release();  // give up the seat at the table
        if (!ate) break;
    }
}

}  // namespace

Summary run(const Config& cfg) {
    Experiment ex(cfg, "waiter");
    std::vector<std::mutex> forks(cfg.n);
    Semaphore waiter_sem(cfg.n - 1);  // the "waiter" lets in at most N-1 people
    return ex.run([&](int id) { philosopher(id, ex, forks, waiter_sem); });
}

}  // namespace waiter
