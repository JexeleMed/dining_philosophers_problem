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
// With no solution given it shows an interactive menu, where the settings can be typed in.
// All times are in ms; a single value (--think 100) means a fixed time.
// Options:
//     --time S        duration of a run in seconds (default 60)
//     --n N           number of philosophers (default 5)
//     --think A-B     thinking time range in ms (default 50-300)
//     --eat A-B       eating time range in ms (default 50-300)
//     --pause A-B     deadlock demo: pause between left and right fork in ms (default 0-600)
//     --seed X        RNG seed (default: random, printed and saved)
//     --glutton       philosopher 0 thinks much shorter than the others
//     --glutton-think A-B  thinking time of the glutton in ms (default 0-30)
//     --verbose       log every event
//     --out DIR       directory for CSV results (default results)

#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "solutions.h"

namespace {

void print_usage() {
    std::cout << "Usage: dining_philosophers_problem [1-5] [--time S] [--n N] [--think A-B]\n"
                 "       [--eat A-B] [--glutton] [--glutton-think A-B] [--pause A-B] [--seed X]\n"
                 "       [--verbose] [--out DIR]\n"
                 "Times are in ms; a single value means a fixed time, e.g. --think 100.\n";
}

void print_menu(const Config& cfg) {
    std::cout << "=== Dining philosophers problem ===\n"
              << "Settings: N=" << cfg.n << ", time=" << cfg.time_s << " s, think="
              << cfg.think_min_ms << "-" << cfg.think_max_ms << " ms, eat=" << cfg.eat_min_ms << "-"
              << cfg.eat_max_ms << " ms, glutton=" << (cfg.glutton ? "yes" : "no") << "\n"
              << "  1) Resource hierarchy        (no deadlock)\n"
              << "  2) Waiter / semaphore N-1    (no deadlock)\n"
              << "  3) Monitor - fair queue      (no starvation)\n"
              << "  4) Naive - deadlock demo\n"
              << "  5) Compare 1, 2 and 3\n"
              << "  6) Change settings\n"
              << "  0) Quit\n"
              << "Choice: ";
}

// std::stoi that rejects trailing characters ("12x") and reports through nullopt.
std::optional<int> to_int(const std::string& s) {
    try {
        size_t used = 0;
        int v = std::stoi(s, &used);
        if (used == s.size()) return v;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

// Accepts "A-B" or a single value "A" (a fixed time).
void parse_range(const std::string& s, int& lo, int& hi) {
    const auto dash = s.find('-');
    const auto a = to_int(s.substr(0, dash));
    const auto b = dash == std::string::npos ? a : to_int(s.substr(dash + 1));
    if (!a || !b) throw std::invalid_argument("expected a time in ms or a range A-B: " + s);
    if (*a < 0 || *b < *a) throw std::invalid_argument("invalid range: " + s);
    lo = *a;
    hi = *b;
}

int parse_int(const std::string& s, int min, const std::string& name) {
    const auto v = to_int(s);
    if (!v) throw std::invalid_argument(name + " must be a number: " + s);
    if (*v < min) throw std::invalid_argument(name + " must be at least " + std::to_string(min));
    return *v;
}

// Reads a line; nullopt on end of input, empty string keeps the current value.
std::optional<std::string> ask(const std::string& prompt) {
    std::cout << prompt << ": " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return std::nullopt;
    return line;
}

void edit_settings(Config& cfg) {
    const auto range = [](int lo, int hi) { return std::to_string(lo) + "-" + std::to_string(hi); };
    std::cout << "Press Enter to keep the current value. Times in ms, A-B or a single value.\n";

    // Every field is re-asked until the input is valid, so a typo loses nothing.
    const auto field = [](const std::string& prompt, const std::function<void(const std::string&)>& apply) {
        while (true) {
            auto line = ask(prompt);
            if (!line || line->empty()) return;
            try {
                apply(*line);
                return;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        }
    };

    field("Number of philosophers [" + std::to_string(cfg.n) + "]",
          [&](const std::string& s) { cfg.n = parse_int(s, 2, "number of philosophers"); });
    field("Run time in seconds [" + std::to_string(cfg.time_s) + "]",
          [&](const std::string& s) { cfg.time_s = parse_int(s, 1, "run time"); });
    field("Thinking time [" + range(cfg.think_min_ms, cfg.think_max_ms) + "]",
          [&](const std::string& s) { parse_range(s, cfg.think_min_ms, cfg.think_max_ms); });
    field("Eating time [" + range(cfg.eat_min_ms, cfg.eat_max_ms) + "]",
          [&](const std::string& s) { parse_range(s, cfg.eat_min_ms, cfg.eat_max_ms); });
    field(std::string("Glutton, y/n [") + (cfg.glutton ? "y" : "n") + "]", [&](const std::string& s) {
        if (s != "y" && s != "n") throw std::invalid_argument("answer y or n");
        cfg.glutton = s == "y";
    });
    if (cfg.glutton) {
        field("Glutton thinking time [" + range(cfg.glutton_think_min_ms, cfg.glutton_think_max_ms) + "]",
              [&](const std::string& s) { parse_range(s, cfg.glutton_think_min_ms, cfg.glutton_think_max_ms); });
    }
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
            if (arg == "--time")         cfg.time_s = parse_int(value(), 1, "--time");
            else if (arg == "--n")       cfg.n = parse_int(value(), 2, "--n");
            else if (arg == "--think")   parse_range(value(), cfg.think_min_ms, cfg.think_max_ms);
            else if (arg == "--eat")     parse_range(value(), cfg.eat_min_ms, cfg.eat_max_ms);
            else if (arg == "--pause")   parse_range(value(), cfg.pause_min_ms, cfg.pause_max_ms);
            else if (arg == "--seed")    cfg.seed = static_cast<unsigned>(std::stoul(value()));
            else if (arg == "--glutton") cfg.glutton = true;
            else if (arg == "--glutton-think")
                parse_range(value(), cfg.glutton_think_min_ms, cfg.glutton_think_max_ms);
            else if (arg == "--verbose") cfg.verbose = true;
            else if (arg == "--out")     cfg.out_dir = value();
            else if (arg == "--help" || arg == "-h") {
                print_usage();
                return 0;
            } else {
                if (arg.rfind("--", 0) == 0) throw std::invalid_argument("unknown option " + arg);
                choice = parse_int(arg, 0, "solution number");
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        print_usage();
        return 1;
    }

    if (cfg.seed == 0) cfg.seed = std::random_device{}();

    if (choice != -1) return dispatch(choice, cfg);

    // Menu
    while (true) {
        print_menu(cfg);
        std::string line;
        if (!std::getline(std::cin, line)) return 0;
        if (line == "0") break;
        if (line == "6") {
            std::cout << "\n";
            edit_settings(cfg);
            std::cout << "\n";
        } else if (line.size() == 1 && line[0] >= '1' && line[0] <= '5') {
            std::cout << "\n";
            dispatch(line[0] - '0', cfg);
            std::cout << "\n";
        } else {
            std::cout << "Invalid choice.\n\n";
        }
    }
    return 0;
}
