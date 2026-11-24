#pragma once

#include <chrono>
#include <cstdint>

#include <termios.h>

#include "cpu.hpp"

// Actions requested from the keyboard
enum class MonitorCommand {
    None,
    TogglePause,  // Space or P
};

// Live TUI view of the CPU core, drawn on stdout with ANSI escape codes.
// Uses the terminal's alternate screen, so the normal scrollback is left
// intact once the monitor is destroyed.
class Monitor {
public:
    // Turns on cpu.record_history. Throws std::runtime_error if stdout isn't a terminal.
    explicit Monitor(Cpu& cpu);
    ~Monitor();

    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    // Redraw the whole screen. `ticks_run` is the number of OSC1 ticks
    // executed so far, `elapsed` the wall-clock run time, `paused` whether
    // the emulator is currently paused.
    void draw(const Cpu& cpu, std::uint64_t ticks_run,
              std::chrono::duration<double> elapsed, bool paused);

    // Read any pending keypresses (never blocks) and return the requested action
    MonitorCommand poll_input();

private:
    // Terminal settings to restore on exit; only valid if keys_enabled_
    termios saved_termios_{};
    bool keys_enabled_ = false;

    // Previous sample, for computing rates between redraws
    std::uint64_t last_ticks_ = 0;
    std::uint64_t last_instrs_ = 0;
    std::uint64_t last_halt_ticks_ = 0;
    double last_elapsed_ = 0.0;

    // Smoothed rates
    double tick_rate_ = 0.0;   // ticks per second
    double instr_rate_ = 0.0;  // instructions per second
    double halt_frac_ = 0.0;   // fraction of ticks spent halted
};
