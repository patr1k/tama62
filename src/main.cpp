#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>
#include <thread>

#include "cpu.hpp"
#include "display.hpp"
#include "monitor.hpp"

namespace {

// 32.768 kHz oscillator (OSC1)
using Ticks = std::chrono::duration<std::int64_t, std::ratio<1, 32768>>;

// Run ticks in ~1 ms batches
constexpr auto batch_period = std::chrono::milliseconds(1);

// If we fall further behind than this (the terminal can't keep up with the
// logs), drop the backlog instead of running flat out to catch up
constexpr Ticks max_backlog = std::chrono::duration_cast<Ticks>(std::chrono::milliseconds(100));

// Redraw the LCD and handle window events at ~60 fps
constexpr auto frame_period = std::chrono::microseconds(16667);

// Redraw the terminal core monitor at ~10 fps
constexpr auto monitor_period = std::chrono::milliseconds(100);

// Set by the SIGINT (Ctrl-C) handler; the run loop checks it between batches
volatile std::sig_atomic_t stop_requested = 0;

void on_sigint(int) {
    stop_requested = 1;
    // A second Ctrl-C kills the program immediately, in case something hangs
    std::signal(SIGINT, SIG_DFL);
}

} // namespace

int main(int argc, char* argv[]) {
    bool headless = false;
    bool monitor_on = false;

    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "--headless") {
            headless = true;
        } else if (arg == "-m" || arg == "--monitor") {
            monitor_on = true;
        } else {
            std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
            std::fprintf(stderr, "Usage: %s [-m | --monitor] [--headless]\n", argv[0]);
            return 1;
        }
    }

    try {
        Cpu cpu("tama.bin");

        // No window in headless mode (e.g. for tests)
        std::unique_ptr<Display> display;
        if (!headless) {
            display = std::make_unique<Display>();
        }

        // Terminal core monitor (restores the terminal when destroyed)
        std::unique_ptr<Monitor> monitor;
        if (monitor_on) {
            monitor = std::make_unique<Monitor>(cpu);
        }

        std::signal(SIGINT, on_sigint);

        using clock = std::chrono::steady_clock;
        const auto start = clock::now();
        Ticks ticks_done{0};
        std::uint64_t ticks_run = 0;
        auto next_frame = start;
        auto next_monitor = start;
        bool window_closed = false;
        bool paused = false;

        while (!stop_requested && !window_closed) {
            Ticks ticks_due = std::chrono::duration_cast<Ticks>(clock::now() - start);

            if (monitor && monitor->poll_input() == MonitorCommand::TogglePause) {
                paused = !paused;
                next_monitor = clock::now();  // show the new state right away
            }

            if (paused) {
                ticks_done = ticks_due;  // let time pass without running it, so resuming doesn't catch up
            } else if (ticks_due - ticks_done > max_backlog) {
                ticks_done = ticks_due - max_backlog;
            }

            for (; ticks_done < ticks_due; ++ticks_done) {
                cpu.tick();
                ticks_run++;
            }

            if (display && clock::now() >= next_frame) {
                window_closed = !display->poll_events();

                LcdFrame frame;
                cpu.lcd_frame(&frame);
                display->present(frame);

                next_frame += frame_period;
                if (next_frame < clock::now()) {
                    next_frame = clock::now() + frame_period;  // fell behind; don't burst
                }
            }

            if (monitor && clock::now() >= next_monitor) {
                monitor->draw(cpu, ticks_run, clock::now() - start, paused);
                next_monitor = clock::now() + monitor_period;
            }

            std::this_thread::sleep_for(batch_period);
        }

        monitor.reset();  // back to the normal screen before printing the summary

        std::chrono::duration<double> elapsed = clock::now() - start;
        std::printf("\nStopped after %llu ticks (%.2f s)\n",
                    static_cast<unsigned long long>(ticks_run), elapsed.count());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    return 0;
}
