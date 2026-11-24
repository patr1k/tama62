#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string_view>
#include <thread>

#include "cpu.hpp"

namespace {

// One CPU tick per period of the 32.768 kHz oscillator (OSC1)
using Ticks = std::chrono::duration<std::int64_t, std::ratio<1, 32768>>;

// Run ticks in ~1 ms batches; sleeping between individual 30 us ticks isn't feasible
constexpr auto batch_period = std::chrono::milliseconds(1);

// If we fall further behind than this (e.g. the terminal can't keep up with the
// logs), drop the backlog instead of running flat out to catch up
constexpr Ticks max_backlog = std::chrono::duration_cast<Ticks>(std::chrono::milliseconds(100));

// Set by the SIGINT (Ctrl-C) handler; the run loop checks it between batches
volatile std::sig_atomic_t stop_requested = 0;

void on_sigint(int) {
    stop_requested = 1;
    // A second Ctrl-C kills the program immediately, in case something hangs
    std::signal(SIGINT, SIG_DFL);
}

} // namespace

int main(int argc, char* argv[]) {
    bool trace = false;

    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "-t" || arg == "--trace") {
            trace = true;
        } else {
            std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
            std::fprintf(stderr, "Usage: %s [-t | --trace]\n", argv[0]);
            return 1;
        }
    }

    try {
        Cpu cpu("tama.bin");
        cpu.trace = trace;

        std::signal(SIGINT, on_sigint);

        using clock = std::chrono::steady_clock;
        const auto start = clock::now();
        Ticks ticks_done{0};
        std::uint64_t ticks_run = 0;

        while (!stop_requested) {
            Ticks ticks_due = std::chrono::duration_cast<Ticks>(clock::now() - start);

            if (ticks_due - ticks_done > max_backlog) {
                ticks_done = ticks_due - max_backlog;
            }

            for (; ticks_done < ticks_due; ++ticks_done) {
                cpu.tick();
                ticks_run++;
            }

            std::this_thread::sleep_for(batch_period);
        }

        std::chrono::duration<double> elapsed = clock::now() - start;
        std::printf("\nStopped after %llu ticks (%.2f s)\n",
                    static_cast<unsigned long long>(ticks_run), elapsed.count());
        cpu.dump();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    return 0;
}
