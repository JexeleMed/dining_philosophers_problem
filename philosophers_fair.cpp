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

#include <condition_variable>
#include <mutex>
#include <vector>

#include "solutions.h"

namespace fair {

namespace {

class Table {
public:
    explicit Table(int n) : n_(n), state_(n, THINKING), ticket_(n, 0) {}

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
        for (int nb : {(i + n_ - 1) % n_, (i + 1) % n_}) {
            if (state_[nb] == EATING) return false;
            if (state_[nb] == HUNGRY && ticket_[nb] < ticket_[i]) return false;
        }
        return true;
    }

    int n_;
    std::mutex m_;
    std::condition_variable cv_;
    std::vector<State> state_;
    std::vector<unsigned long> ticket_;
    unsigned long next_ticket_ = 0;
};

void philosopher(int id, Experiment& ex, Table& table) {
    auto rng = ex.make_rng(id);

    while (!ex.stopped()) {
        ex.think(id, rng);
        if (ex.stopped()) break;

        ex.hungry(id);
        table.pickup(id);
        bool ate = ex.start_eating(id);
        if (ate) ex.eat(id, rng);
        table.putdown(id);
        if (!ate) break;
    }
}

}  // namespace

Summary run(const Config& cfg) {
    Experiment ex(cfg, "fair");
    Table table(cfg.n);
    return ex.run([&](int id) { philosopher(id, ex, table); });
}

}  // namespace fair
