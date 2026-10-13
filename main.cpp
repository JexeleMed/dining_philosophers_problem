// Dining philosophers problem - shared entry point.
//
// Usage:
//     ./dining_philosophers_problem [solution] [options]
// Solutions:
//     1  resource hierarchy               (no deadlock)
//     2  waiter / semaphore N-1           (no deadlock)
//     3  monitor with a fair queue        (no deadlock, no starvation)
//     4  naive - deadlock demo
//     5  compare 1, 2 and 3 with the same configuration
// With no solution given it shows an interactive menu.
// Options:
//     --time S        duration of a run in seconds (default 60)
//     --n N           number of philosophers (default 5)
//     --think A-B     thinking time range in ms (default 10-50)
//     --eat A-B       eating time range in ms (default 10-50)
//     --pause A-B     deadlock demo: pause between left and right fork in ms (default 0-100)
//     --seed X        RNG seed (default: random, printed and saved)
//     --glutton       philosopher 0 thinks only 0-5 ms
//     --verbose       log every event
//     --out DIR       directory for CSV results (default results)

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "solutions.h"

namespace {

void print_usage() {
    std::cout << "Usage: dining_philosophers_problem [1-5] [--time S] [--n N] [--think A-B]\n"
                 "       [--eat A-B] [--pause A-B] [--seed X] [--glutton] [--verbose] [--out DIR]\n";
}

void print_menu() {
    std::cout << "=== Dining philosophers problem ===\n"
              << "  1) Resource hierarchy        (no deadlock)\n"
              << "  2) Waiter / semaphore N-1    (no deadlock)\n"
              << "  3) Monitor - fair queue      (no starvation)\n"
              << "  4) Naive - deadlock demo\n"
              << "  5) Compare 1, 2 and 3\n"
              << "  0) Quit\n"
              << "Choice: ";
}

void parse_range(const std::string& s, int& lo, int& hi) {
    auto dash = s.find('-');
    if (dash == std::string::npos) throw std::invalid_argument("expected range A-B: " + s);
    lo = std::stoi(s.substr(0, dash));
    hi = std::stoi(s.substr(dash + 1));
    if (lo < 0 || hi < lo) throw std::invalid_argument("invalid range: " + s);
}

int dispatch(int choice, const Config& cfg) {
    switch (choice) {
        case 1: hierarchy::run(cfg); return 0;
        case 2: waiter::run(cfg); return 0;
        case 3: fair::run(cfg); return 0;
        case 4: deadlock::run(cfg); return 0;
        case 5: {
            std::vector<Summary> summaries;
            summaries.push_back(hierarchy::run(cfg));
            std::cout << "\n";
            summaries.push_back(waiter::run(cfg));
            std::cout << "\n";
            summaries.push_back(fair::run(cfg));
            print_comparison(summaries);
            return 0;
        }
        default:
            std::cerr << "Unknown choice: " << choice << "\n";
            return 1;
    }
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    int choice = -1;

    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
                return argv[++i];
            };
            if (arg == "--time")         cfg.time_s = std::stoi(value());
            else if (arg == "--n")       cfg.n = std::stoi(value());
            else if (arg == "--think")   parse_range(value(), cfg.think_min_ms, cfg.think_max_ms);
            else if (arg == "--eat")     parse_range(value(), cfg.eat_min_ms, cfg.eat_max_ms);
            else if (arg == "--pause")   parse_range(value(), cfg.pause_min_ms, cfg.pause_max_ms);
            else if (arg == "--seed")    cfg.seed = static_cast<unsigned>(std::stoul(value()));
            else if (arg == "--glutton") cfg.glutton = true;
            else if (arg == "--verbose") cfg.verbose = true;
            else if (arg == "--out")     cfg.out_dir = value();
            else if (arg == "--help" || arg == "-h") {
                print_usage();
                return 0;
            } else {
                choice = std::stoi(arg);
            }
        }
        if (cfg.n < 2) throw std::invalid_argument("--n must be at least 2");
        if (cfg.time_s < 1) throw std::invalid_argument("--time must be at least 1");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        print_usage();
        return 1;
    }

    if (cfg.seed == 0) cfg.seed = std::random_device{}();

    if (choice != -1) return dispatch(choice, cfg);

    // Menu
    while (true) {
        print_menu();
        if (!(std::cin >> choice)) return 0;
        if (choice == 0) break;
        if (choice >= 1 && choice <= 5) {
            std::cout << "\n";
            dispatch(choice, cfg);
            std::cout << "\n";
        } else {
            std::cout << "Invalid choice.\n\n";
        }
    }
    return 0;
}
