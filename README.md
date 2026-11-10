Dining philosophers problem, written as an assignment for the Operating Systems course.

Three solutions (resource hierarchy, waiter with a semaphore, monitor with a fair queue)
and a naive version that demonstrates a deadlock, in one C++20 program.

Build:

    cmake -S . -B build && cmake --build build

Run:

    ./build/dining_philosophers_problem [1-5] [--time S] [--n N] [--think A-B] [--eat A-B]
        [--glutton] [--glutton-think A-B] [--pause A-B] [--seed X] [--verbose] [--out DIR]

    1 hierarchy, 2 waiter, 3 fair monitor, 4 deadlock demo, 5 compare 1-3

Times are in milliseconds, a single value (`--think 100`) means a fixed time. The defaults
(think and eat 50-300 ms, 60 s) give about 100 meals per philosopher at N = 5. Without a
solution number the program shows a menu, where the same settings can be typed in.

Measurements and report data:

    scripts/measure.sh                                    # about 50 minutes, writes to results/v2
    python3 -m venv .venv && .venv/bin/pip install matplotlib
    .venv/bin/python scripts/plots.py                     # charts and tables for the report
