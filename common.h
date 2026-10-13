// Shared measurement infrastructure for all solutions.
//
// Every solution runs for a fixed amount of time (--time), so the number of meals
// each philosopher managed to eat says something about
// fairness / starvation. Each philosopher thread writes only to
// its own PhilosopherStats entry.
#ifndef COMMON_H
#define COMMON_H

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

struct Config {
    int n = 5;               // number of philosophers and forks
    int time_s = 60;         // duration of a single run
    int think_min_ms = 10;
    int think_max_ms = 50;
    int eat_min_ms = 10;
    int eat_max_ms = 50;
    int pause_min_ms = 0;    // deadlock demo: pause between taking the left and right fork
    int pause_max_ms = 100;
    unsigned seed = 0;       // 0 = pick a random one (it is printed and saved)
    bool glutton = false;    // philosopher 0 thinks only 0-5 ms
    bool verbose = false;    // log every event
    std::string out_dir = "results";
};

struct WaitSample {
    double t_ms;     // when the philosopher got hungry, since the start of the run
    double wait_ms;  // how long it took until it started eating
};

struct PhilosopherStats {
    long meals = 0;
    long attempts = 0;             // how many times it got hungry
    double total_wait_ms = 0;
    double min_wait_ms = 0;
    double max_wait_ms = 0;
    double pending_wait_ms = 0;    // unfinished wait at the moment the run stopped
    double total_eat_ms = 0;
    double total_think_ms = 0;
    std::vector<WaitSample> waits;

    bool pending = false;
    Clock::time_point hungry_since;
};

struct Summary {
    std::string run_id;
    std::string solution;
    double elapsed_s = 0;
    long total_meals = 0;
    double throughput = 0;         // meals per second
    long min_meals = 0;
    long max_meals = 0;
    double mean_meals = 0;
    double stddev_meals = 0;
    double max_min_ratio = 0;      // 0 when somebody ate nothing
    double jain = 0;               // Jain's fairness index, 1.0 = perfectly equal
    double avg_wait_ms = 0;
    double max_wait_ms = 0;
    double p50_wait_ms = 0;
    double p95_wait_ms = 0;
    double p99_wait_ms = 0;
    double max_pending_wait_ms = 0;
    double avg_eating = 0;         // average number of philosophers eating at once
    bool deadlock = false;
    double deadlock_at_s = 0;
};

class Experiment {
public:
    Experiment(const Config& cfg, std::string solution);

    const Config& cfg() const { return cfg_; }
    int n() const { return cfg_.n; }
    bool stopped() const { return stop_.load(std::memory_order_relaxed); }

    std::mt19937 make_rng(int id) const;
    void log(int id, const std::string& msg);

    // Building blocks of a philosopher's loop. They sleep and record statistics.
    void think(int id, std::mt19937& rng);
    void hungry(int id);
    bool start_eating(int id);  // false = the run is over, the meal is not counted
    void eat(int id, std::mt19937& rng);

    void mark_deadlock(Clock::time_point at);
    void note(const std::string& msg);  // message from the main thread

    // Starts n philosopher threads, lets them run for cfg.time_s, stops them, then
    // prints the report and saves all measurements to cfg.out_dir.
    Summary run(const std::function<void(int)>& philosopher,
                const std::function<bool()>& watch = {});

private:
    double since_start_ms(Clock::time_point t) const;
    Summary summarize() const;
    void print_report(const Summary& s) const;
    void save(const Summary& s) const;

    Config cfg_;
    std::string solution_;
    std::string run_id_;
    std::vector<PhilosopherStats> stats_;
    std::atomic<bool> stop_{false};
    std::atomic<long> meals_so_far_{0};
    std::mutex out_mtx_;
    Clock::time_point start_;
    Clock::time_point stop_time_;
    bool deadlock_ = false;
    Clock::time_point deadlock_at_;
};

void print_comparison(const std::vector<Summary>& summaries);

#endif  // COMMON_H
