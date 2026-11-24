// Human-readable dump of the CPU state, printed when the emulator is stopped

#include "cpu.hpp"

#include <cstdio>

namespace {

// Print one RAM page as rows of 16 nibbles, labelled with their 12-bit address.
// `valid_row` says which rows (high nibble of the step) actually exist.
template <std::size_t Rows, typename ValidRow>
void dump_page(std::FILE* out, const char* name, uint4_t page,
               const RamPage<Rows>& mem, ValidRow valid_row) {
    std::fprintf(out, "\n%s\n", name);
    std::fprintf(out, "       0 1 2 3 4 5 6 7 8 9 A B C D E F\n");
    for (std::size_t row = 0; row < Rows; row++) {
        if (!valid_row(row)) continue;
        std::fprintf(out, "  %X%zX0:", page, row);
        for (uint4_t nibble : mem[row]) {
            std::fprintf(out, " %X", nibble & 0xF);
        }
        std::fprintf(out, "\n");
    }
}

constexpr auto all_rows = [](std::size_t) { return true; };

} // namespace

void Cpu::dump(std::FILE* out) const {
    const ProgramCounter& pc = reg.PC;

    std::fprintf(out, "\n==================== CPU state ====================\n");

    std::fprintf(out, "PC  %d:%X:%02X   (NBP:NPP %d:%X)", pc.PCB, pc.PCP, pc.PCS, pc.NBP, pc.NPP);
    if ((!pc.PCB && pc.PCP <= 0xF) || (pc.PCB && pc.PCP <= 0x7)) {
        const RomPage& page = pc.PCB ? rom.bank1[pc.PCP] : rom.bank0[pc.PCP];
        std::fprintf(out, "   next opcode %03X", page[pc.PCS]);
    }
    std::fprintf(out, "\n");

    std::fprintf(out, "A   %X     B   %X\n", reg.A & 0xF, reg.B & 0xF);
    std::fprintf(out, "X   %X:%02X  Y   %X:%02X   (XP:XHL, YP:YHL)\n",
                 reg.XP & 0xF, reg.XHL, reg.YP & 0xF, reg.YHL);
    std::fprintf(out, "SP  %02X\n", reg.SP);
    std::fprintf(out, "F   I=%d D=%d Z=%d C=%d  (%X)\n",
                 flags.I, flags.D, flags.Z, flags.C, flags.pack());
    std::fprintf(out, "clock_wait %u   halt %d\n", clock_wait, halt);

    std::fprintf(out, "\n-------------------- Memory ---------------------\n");
    dump_page(out, "RAM page 0 (000-0FF)", 0x0, ram.page0, all_rows);
    dump_page(out, "RAM page 1 (100-1FF)", 0x1, ram.page1, all_rows);
    dump_page(out, "RAM page 2 (200-27F)", 0x2, ram.page2, all_rows);
    dump_page(out, "Display memory (E00-E4F, E80-ECF)", 0xE, ram.pageE,
              [](std::size_t row) { return row < 5 || (row >= 8 && row < 0xD); });
    dump_page(out, "I/O registers (F00-F7F)", 0xF, ram.pageF,
              [](std::size_t row) { return row < 8; });

    std::fprintf(out, "===================================================\n");
    std::fflush(out);
}
