// Interrupt dispatch
//
// Each source sets its interrupt factor flag(s) in F00H-F05H. A source
// requests an interrupt when a factor flag is set, its mask register bit
// (F10H-F15H, same low nibble) is enabled, and the CPU's I flag is set (EI).
//
// The CPU takes the interrupt between instructions:
//   1. push the PC of the next instruction (like CALL),
//   2. clear I (DI), so the handler isn't interrupted again,
//   3. jump to the source's vector on page 1 of the current bank.
// Steps 1-2 take 12 clock cycles. An interrupt also wakes the CPU from HALT.

#include "cpu.hpp"

namespace {

struct IntSource {
    uint4_t l;           // factor flags at F0lH, mask register at F1lH
    bool gated_at_flag;  // the mask stops the flag being set at all
    std::uint8_t vector; // step on page 1
};

// Highest priority first
constexpr std::array<IntSource, 6> int_sources = {{
    {0x2, false, 0x0C},  // Programmable timer (IPT / EIPT)
    {0x3, false, 0x0A},  // Serial interface (ISIO / EISIO)
    // Input ports: the per-pin masks EIK00-EIK13 decide whether IK0/IK1 get
    // set in the first place, so a set flag is already unmasked
    {0x5, true,  0x08},  // Input K10-K13 (IK1)
    {0x4, true,  0x06},  // Input K00-K03 (IK0)
    {0x1, false, 0x04},  // Stopwatch timer (ISW0, ISW1 / EISW0, EISW1)
    {0x0, false, 0x02},  // Clock timer (IT32-IT1 / EIT32-EIT1)
}};

} // namespace

bool Cpu::service_interrupt() {
    // An interrupt can't split PSET from the jump or call it sets up
    if (!flags.I || after_pset) return false;

    for (const IntSource& src : int_sources) {
        uint4_t factor = ram.pageF[0x0][src.l];
        uint4_t mask = src.gated_at_flag ? 0xF : ram.pageF[0x1][src.l];
        if (!(factor & mask)) continue;

        if (record_history) {
            InstrRecord& rec = history[history_pos];
            rec.bank = reg.PC.PCB;
            rec.page = reg.PC.PCP;
            rec.step = reg.PC.PCS;
            rec.opcode = 0;
            std::snprintf(rec.text.data(), rec.text.size(), "INT -> 1%02XH", src.vector);
            history_pos = (history_pos + 1) % history_size;
        }

        halt = false;
        push_pc();
        flags.I = false;

        reg.PC.PCP = 0x1;
        reg.PC.PCS = src.vector;
        reg.PC.NBP = reg.PC.PCB;  // the vector is in the current bank
        reg.PC.NPP = reg.PC.PCP;

        clock_wait = 11;  // 12 cycles including this one
        return true;
    }
    return false;
}
