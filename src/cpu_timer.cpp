// Timers: clock timer, stopwatch timer, programmable timer
//
// All three run off the OSC1 (32.768 kHz) divider chain, so they keep
// counting while the CPU is in HALT or waiting out a multi-cycle instruction.
// Timer data lives in the page F I/O registers; the CPU can read it but the
// io_write masks make it read-only to software.
//
// Each timer sets its interrupt factor flags (F00H-F02H) whether or not the
// interrupt is masked. Dispatching the interrupt is not handled here.

#include "cpu.hpp"

namespace {

// Clock timer: falling edges of these TM bits raise IT32 / IT8 / IT2 / IT1
constexpr uint8_t TM_32HZ = 0x04;
constexpr uint8_t TM_8HZ  = 0x10;
constexpr uint8_t TM_2HZ  = 0x40;
constexpr uint8_t TM_1HZ  = 0x80;

// Interrupt factor flag bits
constexpr uint8_t IT32 = 0x1;
constexpr uint8_t IT8  = 0x2;
constexpr uint8_t IT2  = 0x4;
constexpr uint8_t IT1  = 0x8;
constexpr IoReg ISW0  {0x0, 0x1, 0x1};
constexpr IoReg ISW1  {0x0, 0x1, 0x2};
constexpr IoReg IPT   {0x0, 0x2, 0x1};

// Control register bits
constexpr IoReg TMRST  {0x7, 0x6, 0x2};
constexpr IoReg SWRUN  {0x7, 0x7, 0x1};
constexpr IoReg SWRST  {0x7, 0x7, 0x2};
constexpr IoReg PTRUN  {0x7, 0x8, 0x1};
constexpr IoReg PTRST  {0x7, 0x8, 0x2};


// The stopwatch counts an approximate 100 Hz signal derived from 256 Hz.
// SWL steps every 2 or 3 /256 s, so each 1/10 s takes either 25 or 26 /256 s;
// SWH alternates those so that one full second is exactly 256 /256 s
constexpr std::array<std::uint8_t, 10> swl_steps_25 = {3, 2, 3, 2, 3, 2, 3, 2, 3, 2};
constexpr std::array<std::uint8_t, 10> swl_steps_26 = {3, 3, 3, 2, 3, 2, 3, 2, 3, 2};
constexpr std::array<bool, 10> swh_long = {true, true, false, false, true, true, false, false, true, true};

// Programmable timer input clock period in OSC1 ticks, indexed by PTC0-PTC2.
// 0 and 1 select the K03 input port, which isn't emulated yet
constexpr std::array<std::uint8_t, 8> pt_period = {0, 0, 128, 64, 32, 16, 8, 4};

} // namespace

void Cpu::clock_timer_set(std::uint8_t tm) {
    std::uint8_t old = io_read8(0x2, 0x0);
    std::uint8_t fell = old & ~tm;

    uint4_t flags = 0;
    if (fell & TM_32HZ) flags |= IT32;
    if (fell & TM_8HZ)  flags |= IT8;
    if (fell & TM_2HZ)  flags |= IT2;
    if (fell & TM_1HZ)  flags |= IT1;
    ram.pageF[0x0][0x0] |= flags;

    io_write8(0x2, 0x0, tm);
}

void Cpu::stopwatch_step() {
    uint4_t& swl = ram.pageF[0x2][0x2];
    uint4_t& swh = ram.pageF[0x2][0x3];

    const auto& steps = swh_long[swh % 10] ? swl_steps_26 : swl_steps_25;
    if (++timers.sw_div < steps[swl % 10]) return;
    timers.sw_div = 0;

    if (++swl < 10) return;
    swl = 0;
    io_write(ISW0, true); // 10 Hz: SWL overflow

    if (++swh < 10) return;
    swh = 0;
    io_write(ISW1, true); // 1 Hz: SWH overflow
}

void Cpu::prog_timer_step() {
    std::uint8_t pt = io_read8(0x2, 0x4) - 1;
    if (pt == 0) {
        io_write(IPT, true);
        pt = io_read8(0x2, 0x6);  // reload (00H gives a 256-count period)
    }
    io_write8(0x2, 0x4, pt);
}

void Cpu::timers_tick() {
    // 7-bit prescaler: OSC1 / 128 = 256 Hz
    timers.osc_div = (timers.osc_div + 1) & 0x7F;

    // The programmable timer's 256-8192 Hz clocks all come off the same
    // divider, so their falling edges line up with it
    std::uint8_t period = pt_period[ram.pageF[0x7][0x9] & 0x7];
    if (io_read(PTRUN) && period && (timers.osc_div & (period - 1)) == 0) {
        prog_timer_step();
    }

    if (timers.osc_div != 0) return;

    // 256 Hz
    clock_timer_set(io_read8(0x2, 0x0) + 1);

    if (io_read(SWRUN)) {
        stopwatch_step();
    }
}

void Cpu::timers_io_write(uint4_t h, uint4_t l) {
    if (h != 0x7) return;

    uint4_t& r = ram.pageF[h][l];
    switch (l) {
        case 0x6:  // F76H: TMRST / WDRST
            // Resetting can produce falling edges, so the IT flags may get set.
            // The watchdog isn't emulated, so WDRST does nothing.
            if (r & TMRST.mask) clock_timer_set(0);
            r = 0;  // write-only bits always read as 0
            break;
        case 0x7:  // F77H: SWRST / SWRUN
            if (r & SWRST.mask) {
                io_write8(0x2, 0x2, 0);
                timers.sw_div = 0;
            }
            r &= ~SWRST.mask;
            break;
        case 0x8:  // F78H: PTRST / PTRUN
            if (r & PTRST.mask) io_write8(0x2, 0x4, io_read8(0x2, 0x6));
            r &= ~PTRST.mask;
            break;
    }
}
