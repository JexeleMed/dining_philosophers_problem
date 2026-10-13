// Dining philosophers problem - shared entry point.
//
// Runs one of the three solutions. The choice is given as a command-line arg:
//     ./dining_philosophers_problem 1    # resource hierarchy
//     ./dining_philosophers_problem 2    # waiter (semaphore N-1)
//     ./dining_philosophers_problem 3    # monitor with a fair queue (no starvation)
// With no argument it shows an interactive menu.

#include <iostream>
#include <string>

#include "solutions.h"

namespace {

void print_menu() {
    std::cout << "=== Dining philosophers problem ===\n"
              << "  1) Resource hierarchy        (no deadlock)\n"
              << "  2) Waiter / semaphore N-1    (no deadlock)\n"
              << "  3) Monitor - fair queue      (no starvation)\n"
              << "  0) Quit\n"
              << "Choice: ";
}

int dispatch(int choice) {
    switch (choice) {
        case 1: return hierarchy::run();
        case 2: return waiter::run();
        case 3: return fair::run();
        default:
            std::cerr << "Unknown choice: " << choice << "\n";
            return 1;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        return dispatch(std::stoi(argv[1]));
    }

    // Menu
    int choice = -1;
    while (choice != 0) {
        print_menu();
        if (!(std::cin >> choice)) return 0;  // e.g. EOF
        if (choice == 0) break;
        if (choice >= 1 && choice <= 3) {
            std::cout << "\n";
            dispatch(choice);
            std::cout << "\n";
        } else {
            std::cout << "Invalid choice.\n\n";
        }
    }
    return 0;
}
