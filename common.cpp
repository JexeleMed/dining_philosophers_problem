#include "common.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace {

double to_ms(Clock::duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

void sleep_random_ms(std::mt19937& rng, int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    std::this_thread::sleep_for(std::chrono::milliseconds(d(rng)));
}

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::ostringstream os;
    os << std::put_time(std::localtime(&t), "%Y%m%d-%H%M%S") << "-" << std::setw(3)
       << std::setfill('0') << ms.count();
    return os.str();
}

double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0;
    auto idx = static_cast<size_t>(std::ceil(p * sorted.size())) - 1;
    return sorted[std::min(idx, sorted.size() - 1)];
}

}  // namespace

Experiment::Experiment(const Config& cfg, std::string solution)
    : cfg_(cfg), solution_(std::move(solution)), stats_(cfg.n) {
    run_id_ = timestamp() + "_" + solution_;
}

std::mt19937 Experiment::make_rng(int id) const {
    return std::mt19937(cfg_.seed + static_cast<unsigned>(id));
}

void Experiment::log(int id, const std::string& msg) {
    if (!cfg_.verbose) return;
    std::lock_guard<std::mutex> lk(out_mtx_);
    std::cout << "[" << std::fixed << std::setprecision(3) << std::setw(8)
              << since_start_ms(Clock::now()) / 1000 << " s] Philosopher " << id << " "
              << msg << std::endl;
}

void Experiment::think(int id, std::mt19937& rng) {
    log(id, "is thinking");
    auto t0 = Clock::now();
    if (cfg_.glutton && id == 0) sleep_random_ms(rng, 0, 5);
    else                         sleep_random_ms(rng, cfg_.think_min_ms, cfg_.think_max_ms);
    stats_[id].total_think_ms += to_ms(Clock::now() - t0);
}

void Experiment::hungry(int id) {
    auto& s = stats_[id];
    ++s.attempts;
    s.pending = true;
    s.hungry_since = Clock::now();
    log(id, "is hungry");
}

bool Experiment::start_eating(int id) {
    if (stopped()) return false;

    auto& s = stats_[id];
    double wait = to_ms(Clock::now() - s.hungry_since);
    s.pending = false;
    s.waits.push_back({since_start_ms(s.hungry_since), wait});
    s.total_wait_ms += wait;
    s.min_wait_ms = s.waits.size() == 1 ? wait : std::min(s.min_wait_ms, wait);
    s.max_wait_ms = std::max(s.max_wait_ms, wait);
    log(id, "is eating");
    return true;
}

void Experiment::eat(int id, std::mt19937& rng) {
    auto t0 = Clock::now();
    sleep_random_ms(rng, cfg_.eat_min_ms, cfg_.eat_max_ms);
    auto& s = stats_[id];
    s.total_eat_ms += to_ms(Clock::now() - t0);
    ++s.meals;
    ++meals_so_far_;
}

void Experiment::mark_deadlock(Clock::time_point at) {
    deadlock_ = true;
    deadlock_at_ = at;
}

void Experiment::note(const std::string& msg) {
    std::lock_guard<std::mutex> lk(out_mtx_);
    std::cout << "\n" << msg << std::endl;
}

double Experiment::since_start_ms(Clock::time_point t) const {
    return to_ms(t - start_);
}

Summary Experiment::run(const std::function<void(int)>& philosopher,
                        const std::function<bool()>& watch) {
    std::cout << "=== " << solution_ << " ===  n=" << cfg_.n << ", time=" << cfg_.time_s
              << " s, think=" << cfg_.think_min_ms << "-" << cfg_.think_max_ms
              << " ms, eat=" << cfg_.eat_min_ms << "-" << cfg_.eat_max_ms
              << " ms, glutton=" << (cfg_.glutton ? "yes" : "no") << ", seed=" << cfg_.seed;
    if (solution_ == "deadlock")
        std::cout << ", pause=" << cfg_.pause_min_ms << "-" << cfg_.pause_max_ms << " ms";
    std::cout << std::endl;

    start_ = Clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < cfg_.n; ++i) threads.emplace_back(philosopher, i);

    const auto deadline = start_ + std::chrono::seconds(cfg_.time_s);
    auto next_progress = start_ + std::chrono::seconds(1);
    while (Clock::now() < deadline) {
        std::this_thread::sleep_until(std::min(Clock::now() + std::chrono::milliseconds(50), deadline));
        if (watch && watch()) break;
        if (!cfg_.verbose && Clock::now() >= next_progress) {
            std::lock_guard<std::mutex> lk(out_mtx_);
            std::cout << "\r  " << static_cast<int>(since_start_ms(Clock::now()) / 1000) << "/"
                      << cfg_.time_s << " s, meals: " << meals_so_far_ << std::flush;
            next_progress += std::chrono::seconds(1);
        }
    }
    stop_time_ = Clock::now();
    stop_ = true;
    for (auto& t : threads) t.join();
    if (!cfg_.verbose) std::cout << "\n";

    for (auto& s : stats_) {
        if (s.pending) s.pending_wait_ms = to_ms(stop_time_ - s.hungry_since);
    }

    Summary s = summarize();
    print_report(s);
    save(s);
    return s;
}

Summary Experiment::summarize() const {
    Summary s;
    s.run_id = run_id_;
    s.solution = solution_;
    s.elapsed_s = to_ms(stop_time_ - start_) / 1000;

    std::vector<double> all_waits;
    double sum_sq = 0;
    double total_wait = 0;
    double total_eat = 0;
    s.min_meals = stats_.front().meals;
    for (const auto& p : stats_) {
        s.total_meals += p.meals;
        s.min_meals = std::min(s.min_meals, p.meals);
        s.max_meals = std::max(s.max_meals, p.meals);
        sum_sq += static_cast<double>(p.meals) * p.meals;
        total_wait += p.total_wait_ms;
        total_eat += p.total_eat_ms;
        s.max_wait_ms = std::max(s.max_wait_ms, p.max_wait_ms);
        s.max_pending_wait_ms = std::max(s.max_pending_wait_ms, p.pending_wait_ms);
        for (const auto& w : p.waits) all_waits.push_back(w.wait_ms);
    }

    const double n = cfg_.n;
    s.throughput = s.total_meals / s.elapsed_s;
    s.mean_meals = s.total_meals / n;
    s.stddev_meals = std::sqrt(std::max(0.0, sum_sq / n - s.mean_meals * s.mean_meals));
    s.max_min_ratio = s.min_meals > 0 ? static_cast<double>(s.max_meals) / s.min_meals : 0;
    s.jain = sum_sq > 0 ? static_cast<double>(s.total_meals) * s.total_meals / (n * sum_sq) : 0;
    s.avg_eating = total_eat / (s.elapsed_s * 1000);

    if (!all_waits.empty()) {
        std::sort(all_waits.begin(), all_waits.end());
        s.avg_wait_ms = total_wait / all_waits.size();
        s.p50_wait_ms = percentile(all_waits, 0.50);
        s.p95_wait_ms = percentile(all_waits, 0.95);
        s.p99_wait_ms = percentile(all_waits, 0.99);
    }

    s.deadlock = deadlock_;
    if (deadlock_) s.deadlock_at_s = since_start_ms(deadlock_at_) / 1000;
    return s;
}

void Experiment::print_report(const Summary& s) const {
    const double elapsed_ms = s.elapsed_s * 1000;

    std::cout << "\n" << std::fixed << std::setprecision(2)
              << " id   meals  attempts  avg wait  min wait  max wait   pending   eat %   wait %\n"
              << "                          [ms]      [ms]      [ms]      [ms]\n";
    for (int i = 0; i < cfg_.n; ++i) {
        const auto& p = stats_[i];
        double avg = p.meals > 0 ? p.total_wait_ms / p.meals : 0;
        std::cout << std::setw(3) << i << std::setw(8) << p.meals << std::setw(10) << p.attempts
                  << std::setw(10) << avg << std::setw(10) << p.min_wait_ms
                  << std::setw(10) << p.max_wait_ms << std::setw(10) << p.pending_wait_ms
                  << std::setw(8) << 100 * p.total_eat_ms / elapsed_ms
                  << std::setw(9) << 100 * (p.total_wait_ms + p.pending_wait_ms) / elapsed_ms
                  << "\n";
    }

    std::cout << "\n--- Summary (" << s.solution << ") ---\n"
              << "elapsed:               " << s.elapsed_s << " s\n"
              << "total meals:           " << s.total_meals << "\n"
              << "throughput:            " << s.throughput << " meals/s\n"
              << "meals min/mean/max:    " << s.min_meals << " / " << s.mean_meals << " / "
              << s.max_meals << "  (stddev " << s.stddev_meals << ")\n"
              << "max/min ratio:         " << s.max_min_ratio << "\n"
              << "Jain fairness index:   " << std::setprecision(4) << s.jain
              << std::setprecision(2) << "  (1.0 = equal, " << 1.0 / cfg_.n
              << " = one eats everything)\n"
              << "wait avg/p50/p95/p99:  " << s.avg_wait_ms << " / " << s.p50_wait_ms << " / "
              << s.p95_wait_ms << " / " << s.p99_wait_ms << " ms\n"
              << "wait max:              " << s.max_wait_ms << " ms\n"
              << "longest unfinished:    " << s.max_pending_wait_ms << " ms\n"
              << "avg eating at once:    " << s.avg_eating << "  (max possible " << cfg_.n / 2
              << ")\n";

    std::cout << "Verdict: ";
    if (s.deadlock) {
        std::cout << "DEADLOCK detected after " << s.deadlock_at_s
                  << " s - every philosopher holds one fork and waits forever.\n";
    } else if (solution_ == "deadlock") {
        std::cout << "no deadlock in this run - it is random, try again or use a longer --pause.\n";
    } else if (s.min_meals == 0) {
        std::cout << "STARVATION - philosopher(s)";
        for (int i = 0; i < cfg_.n; ++i)
            if (stats_[i].meals == 0) std::cout << " " << i;
        std::cout << " ate nothing.\n";
    } else if (s.min_meals < 0.5 * s.mean_meals) {
        std::cout << "possible starvation - philosopher(s)";
        for (int i = 0; i < cfg_.n; ++i)
            if (stats_[i].meals < 0.5 * s.mean_meals) std::cout << " " << i;
        std::cout << " ate less than half of the average.\n";
    } else {
        std::cout << "no deadlock, nobody was starved.\n";
    }
}

void Experiment::save(const Summary& s) const {
    namespace fs = std::filesystem;
    fs::create_directories(cfg_.out_dir);
    const fs::path dir = cfg_.out_dir;

    // One row per run, appended - all runs end up in one table.
    const fs::path runs_path = dir / "runs.csv";
    const bool new_file = !fs::exists(runs_path);
    std::ofstream runs(runs_path, std::ios::app);
    if (new_file) {
        runs << "run_id,solution,n,time_s,elapsed_s,think_min_ms,think_max_ms,eat_min_ms,"
                "eat_max_ms,pause_min_ms,pause_max_ms,glutton,seed,total_meals,throughput,min_meals,max_meals,mean_meals,"
                "stddev_meals,max_min_ratio,jain,avg_wait_ms,p50_wait_ms,p95_wait_ms,"
                "p99_wait_ms,max_wait_ms,max_pending_wait_ms,avg_eating,deadlock,deadlock_at_s\n";
    }
    runs << std::setprecision(6) << s.run_id << "," << s.solution << "," << cfg_.n << ","
         << cfg_.time_s << "," << s.elapsed_s << "," << cfg_.think_min_ms << ","
         << cfg_.think_max_ms << "," << cfg_.eat_min_ms << "," << cfg_.eat_max_ms << ","
         << cfg_.pause_min_ms << "," << cfg_.pause_max_ms << "," << cfg_.glutton << "," << cfg_.seed << "," << s.total_meals << "," << s.throughput
         << "," << s.min_meals << "," << s.max_meals << "," << s.mean_meals << ","
         << s.stddev_meals << "," << s.max_min_ratio << "," << s.jain << "," << s.avg_wait_ms
         << "," << s.p50_wait_ms << "," << s.p95_wait_ms << "," << s.p99_wait_ms << ","
         << s.max_wait_ms << "," << s.max_pending_wait_ms << "," << s.avg_eating << ","
         << s.deadlock << "," << s.deadlock_at_s << "\n";

    std::ofstream phil(dir / (s.run_id + "_philosophers.csv"));
    phil << std::setprecision(6)
         << "id,meals,attempts,total_wait_ms,avg_wait_ms,min_wait_ms,max_wait_ms,"
            "pending_wait_ms,total_eat_ms,total_think_ms\n";
    for (int i = 0; i < cfg_.n; ++i) {
        const auto& p = stats_[i];
        phil << i << "," << p.meals << "," << p.attempts << "," << p.total_wait_ms << ","
             << (p.meals > 0 ? p.total_wait_ms / p.meals : 0) << "," << p.min_wait_ms << ","
             << p.max_wait_ms << "," << p.pending_wait_ms << "," << p.total_eat_ms << ","
             << p.total_think_ms << "\n";
    }

    // Every single wait - for histograms and wait-over-time plots.
    std::ofstream waits(dir / (s.run_id + "_waits.csv"));
    waits << std::setprecision(6) << "id,meal,t_hungry_ms,wait_ms\n";
    for (int i = 0; i < cfg_.n; ++i) {
        const auto& w = stats_[i].waits;
        for (size_t k = 0; k < w.size(); ++k)
            waits << i << "," << k + 1 << "," << w[k].t_ms << "," << w[k].wait_ms << "\n";
    }

    std::cout << "Saved: " << (dir / s.run_id).string() << "_{philosophers,waits}.csv, "
              << runs_path.string() << "\n";
}

void print_comparison(const std::vector<Summary>& summaries) {
    std::cout << "\n=== Comparison ===\n" << std::fixed << std::setprecision(2)
              << std::left << std::setw(12) << "solution" << std::right
              << std::setw(8) << "meals" << std::setw(10) << "meals/s" << std::setw(6) << "min"
              << std::setw(6) << "max" << std::setw(8) << "max/min" << std::setw(8) << "Jain"
              << std::setw(10) << "avg wait" << std::setw(10) << "p99 wait" << std::setw(10)
              << "max wait" << std::setw(8) << "eating" << "\n";
    for (const auto& s : summaries) {
        std::cout << std::left << std::setw(12) << s.solution << std::right
                  << std::setw(8) << s.total_meals << std::setw(10) << s.throughput
                  << std::setw(6) << s.min_meals << std::setw(6) << s.max_meals
                  << std::setw(8) << s.max_min_ratio << std::setw(8) << std::setprecision(4)
                  << s.jain << std::setprecision(2) << std::setw(10) << s.avg_wait_ms
                  << std::setw(10) << s.p99_wait_ms << std::setw(10) << s.max_wait_ms
                  << std::setw(8) << s.avg_eating << "\n";
    }
    std::cout << "(wait times in ms)\n";
}
