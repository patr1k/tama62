#include "cpu.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace {

// Readable/writable bits for each I/O register in page F, one nibble per step.
// Indexed by the high nibble of the step; the low nibble selects the nibble within.
constexpr std::array<std::uint64_t, 16> io_readable = {
    0x0000000000FFFFFF,
    0x0000000000FFFFFF,
    0x00000000FFFFFFFF,
    0x00000000000000FF,
    0x0000000000000FFF,
    0x00000000000FFFFF,
    0x000000000000FFFF,
    0X0FF0F7FDDCBFFFFF,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
};

constexpr std::array<std::uint64_t, 16> io_writable = {
    0x0000000000000000,
    0x0000000000FF113F,
    0x00000000FF000000,
    0x00000000000000FF,
    0x00000000000000F0,
    0x00000000000FFFFF,
    0x000000000000FFFF,
    0x0FF0FFF333FF7FFF,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
    0x0000000000000000,
};

} // namespace

Cpu::Cpu(const std::string& rom_path) {
    reg.PC.reset();

    // Initialize IO registers (everything else starts at zero)
    ram.pageF[4][0] = 0xF;  // K00-K03: pulled up, no buttons pressed
    ram.pageF[4][1] = 0xF;
    ram.pageF[4][2] = 0xF;  // K10-K13: pulled up
    ram.pageF[5][4] = 0xF;
    ram.pageF[7][1] = 0x8;
    ram.pageF[7][3] = 0x8;
    ram.pageF[7][5] = 0x4;
    // F76H-F78H: the timer reset bits are write-only and read as 0, and the
    // timers themselves start out cleared, so these stay zero

    // Flash ROM
    std::ifstream file(rom_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open ROM file: " + rom_path);
    }

    RomAddr addr;
    char buf[2];

    while (file.read(buf, 2)) {
        // Words are stored big-endian
        std::uint16_t word = (static_cast<std::uint8_t>(buf[0]) << 8) | static_cast<std::uint8_t>(buf[1]);
        RomPage& page = addr.bank ? rom.bank1[addr.page] : rom.bank0[addr.page];
        page[addr.step] = word;
        addr.advance();
    }
}

void Cpu::mem_write(uint4_t page, std::uint8_t step, uint4_t val) {
    uint4_t H = hi(step);
    uint4_t L = lo(step);
    if (page == 0) {
        ram.page0[H][L] = val;
    } else if (page == 1) {
        ram.page1[H][L] = val;
    } else if (page == 2) {
        if (H < 8) {
            ram.page2[H][L] = val;
        } else {
            std::printf("Cannot write to page 2, step %X%XH\n", H, L);
        }
    } else if (page > 2 && page < 0xE) {
        std::printf("Cannot write to page %X\n", page);
    } else if (page == 0xE) {
        if (H < 5 || (H > 7 && H < 0xD)) {
            ram.pageE[H][L] = val;
        } else {
            std::printf("Cannot write to page E, step %X%XH\n", H, L);
        }
    } else if (page == 0xF) {
        uint4_t mask = (io_writable[H] >> (L * 4)) & 0xF;
        uint4_t& io_reg = ram.pageF[H][L];

        io_reg = (io_reg & ~mask) | (val & mask);
        timers_io_write(H, L);
        if ((H == 0x1 && (L == 0x4 || L == 0x5)) || (H == 0x4 && L == 0x1)) {
            input_update();
        }
    } else {
        std::printf("Cannot write to page %X, step %X%XH\n", page, H, L);
    }
}

uint4_t Cpu::mem_read(uint4_t page, std::uint8_t step) {
    uint4_t H = hi(step);
    uint4_t L = lo(step);
    if (page == 0) {
        return ram.page0[H][L];
    } else if (page == 1) {
        return ram.page1[H][L];
    } else if (page == 2) {
        if (H < 8) {
            return ram.page2[H][L];
        } else {
            std::printf("Cannot read from page 2, step %X%XH\n", H, L);
            return 0xF;
        }
    } else if (page > 2 && page < 0xE) {
        std::printf("Cannot read from page %X\n", page);
        return 0xF;
    } else if (page == 0xE) {
        if (H < 5 || (H > 7 && H < 0xD)) {
            return ram.pageE[H][L];
        } else {
            std::printf("Cannot read from page E, step %X%XH\n", H, L);
            return 0xF;
        }
    } else if (page == 0xF) {
        // TODO: Implement check for readability (see io_read)
        uint4_t val = ram.pageF[H][L];
        // F00H-F05H: interrupt factor flags are cleared by reading them
        if (H == 0 && L < 6) ram.pageF[H][L] = 0;
        return val;
    } else {
        std::printf("Cannot read from page %X, step %X%XH\n", page, H, L);
        return 0xF;
    }
}

uint4_t Cpu::io_read(IoReg r) const { 
    return ram.pageF[r.h][r.l] & r.mask; 
}

void Cpu::io_write(IoReg r, bool v) {
    if (v) {
        ram.pageF[r.h][r.l] |= r.mask;
    } else {
        ram.pageF[r.h][r.l] &= ~r.mask;
    }
}

std::uint8_t Cpu::io_read8(uint4_t h, uint4_t l) const {
    return (ram.pageF[h][l + 1] << 4) | ram.pageF[h][l];
}

void Cpu::io_write8(uint4_t h, uint4_t l, std::uint8_t v) {
    ram.pageF[h][l] = lo(v);
    ram.pageF[h][l + 1] = hi(v);
}

uint4_t Cpu::reg_read(uint4_t reg_id) {
    switch (reg_id) {
        case 0: return reg.A;
        case 1: return reg.B;
        case 2: return mem_read(reg.XP, reg.XHL);
        case 3: return mem_read(reg.YP, reg.YHL);
    }
    return 0xF;
}

void Cpu::reg_write(uint4_t reg_id, uint4_t val) {
    switch (reg_id) {
        case 0: reg.A = val; return;
        case 1: reg.B = val; return;
        case 2: mem_write(reg.XP, reg.XHL, val); return;
        case 3: mem_write(reg.YP, reg.YHL, val); return;
    }
}

void Cpu::log_instr(const char* fmt, ...) {
    if (!record_history) return;

    va_list args;
    va_start(args, fmt);
    std::array<char, 24>& text = history[history_pos].text;
    std::vsnprintf(text.data(), text.size(), fmt, args);
    va_end(args);
    // Drop the trailing newline the instruction formats end with
    if (char* nl = std::strchr(text.data(), '\n')) *nl = '\0';
}

std::uint16_t Cpu::prefetch() {
    const RomPage& page = reg.PC.PCB ? rom.bank1[reg.PC.PCP] : rom.bank0[reg.PC.PCP];
    std::uint16_t instr = page[reg.PC.PCS];

    reg.PC.advance();
    return instr;
}

void Cpu::stack_push(uint4_t val) {
    std::uint8_t SP = --reg.SP;
    ram.page0[hi(SP)][lo(SP)] = val;
}

uint4_t Cpu::stack_pop() {
    std::uint8_t SP = reg.SP++;
    return ram.page0[hi(SP)][lo(SP)];
}

void Cpu::jump(std::uint8_t step) {
    reg.PC.jump(step);
}

void Cpu::push_pc() {
    stack_push(reg.PC.PCP);
    stack_push(hi(reg.PC.PCS));
    stack_push(lo(reg.PC.PCS));
}

void Cpu::pop_pc() {
    uint4_t PCSL = stack_pop();
    uint4_t PCSH = stack_pop();
    reg.PC.call(stack_pop(), (PCSH << 4) | PCSL);
}


void Cpu::tick() {
    // Timers run off OSC1, so they keep going during HALT and multi-cycle ops
    timers_tick();

    if (clock_wait) { clock_wait--; return; }

    // Interrupts are taken between instructions, and wake the CPU from HALT
    if (service_interrupt()) return;

    if (halt) { halt_ticks++; return; }

    if (record_history) {
        InstrRecord& rec = history[history_pos];
        rec.bank = reg.PC.PCB;
        rec.page = reg.PC.PCP;
        rec.step = reg.PC.PCS;
        std::snprintf(rec.text.data(), rec.text.size(), "???");
    }

    std::uint16_t instr = prefetch();
    instr_count++;

    uint4_t instr_H4 = instr >> 8;
    std::uint8_t instr_H6 = instr >> 6;
    std::uint8_t instr_H7 = instr >> 5;
    std::uint8_t instr_H8 = instr >> 4;
    std::uint16_t instr_H10 = instr >> 2;

    // Decode. Order matters where encodings overlap: e.g. INC X (EE0) must be
    // checked before LDPX r, q (EEx), and SCF etc. before SET F / RST F.
    if      (instr_H7 == 0x72)  op_pset(instr);
    else if (instr_H4 == 0x0)   op_jp(instr);
    else if (instr_H4 == 0x2)   op_jp_c(instr);
    else if (instr_H4 == 0x3)   op_jp_nc(instr);
    else if (instr_H4 == 0x6)   op_jp_z(instr);
    else if (instr_H4 == 0x7)   op_jp_nz(instr);
    else if (instr == 0x0FE8)   op_jpba(instr);
    else if (instr_H4 == 0x4)   op_call(instr);
    else if (instr_H4 == 0x5)   op_calz(instr);
    else if (instr == 0x0FDF)   op_ret(instr);
    else if (instr == 0x0FDE)   op_rets(instr);
    else if (instr_H4 == 0x1)   op_retd(instr);
    else if (instr == 0x0FFB)   op_nop5(instr);
    else if (instr == 0x0FFF)   op_nop7(instr);
    else if (instr == 0x0FF8)   op_halt(instr);
    else if (instr == 0x0EE0)   op_inc_x(instr);
    else if (instr == 0x0EF0)   op_inc_y(instr);
    else if (instr_H4 == 0xB)   op_ld_x_i(instr);
    else if (instr_H4 == 0x8)   op_ld_y_i(instr);
    else if (instr_H10 == 0x3A0) op_ld_xp_r(instr);
    else if (instr_H10 == 0x3A1) op_ld_xh_r(instr);
    else if (instr_H10 == 0x3A2) op_ld_xl_r(instr);
    else if (instr_H10 == 0x3A4) op_ld_yp_r(instr);
    else if (instr_H10 == 0x3A5) op_ld_yh_r(instr);
    else if (instr_H10 == 0x3A6) op_ld_yl_r(instr);
    else if (instr_H10 == 0x3A8) op_ld_r_xp(instr);
    else if (instr_H10 == 0x3A9) op_ld_r_xh(instr);
    else if (instr_H10 == 0x3AA) op_ld_r_xl(instr);
    else if (instr_H10 == 0x3AC) op_ld_r_yp(instr);
    else if (instr_H10 == 0x3AD) op_ld_r_yh(instr);
    else if (instr_H10 == 0x3AE) op_ld_r_yl(instr);
    else if (instr_H8 == 0xA0)  op_adc_xh_i(instr);
    else if (instr_H8 == 0xA1)  op_adc_xl_i(instr);
    else if (instr_H8 == 0xA2)  op_adc_yh_i(instr);
    else if (instr_H8 == 0xA3)  op_adc_yl_i(instr);
    else if (instr_H8 == 0xA4)  op_cp_xh_i(instr);
    else if (instr_H8 == 0xA5)  op_cp_xl_i(instr);
    else if (instr_H8 == 0xA6)  op_cp_yh_i(instr);
    else if (instr_H8 == 0xA7)  op_cp_yl_i(instr);
    else if (instr_H6 == 0x38)  op_ld_r_i(instr);
    else if (instr_H8 == 0xEC)  op_ld_r_q(instr);
    else if (instr_H8 == 0xFA)  op_ld_a_mn(instr);
    else if (instr_H8 == 0xFB)  op_ld_b_mn(instr);
    else if (instr_H8 == 0xF8)  op_ld_mn_a(instr);
    else if (instr_H8 == 0xF9)  op_ld_mn_b(instr);
    else if (instr_H8 == 0xE6)  op_ldpx_mx_i(instr);
    else if (instr_H8 == 0xEE)  op_ldpx_r_q(instr);
    else if (instr_H8 == 0xE7)  op_ldpy_my_i(instr);
    else if (instr_H8 == 0xEF)  op_ldpy_r_q(instr);
    else if (instr_H4 == 0x9)   op_lbpx_mx_l(instr);
    else if (instr == 0x0F41)   op_scf(instr);
    else if (instr == 0x0F5E)   op_rcf(instr);
    else if (instr == 0x0F42)   op_szf(instr);
    else if (instr == 0x0F5D)   op_rzf(instr);
    else if (instr == 0x0F44)   op_sdf(instr);
    else if (instr == 0x0F5B)   op_rdf(instr);
    else if (instr == 0x0F48)   op_ei(instr);
    else if (instr == 0x0F57)   op_di(instr);
    else if (instr_H8 == 0xF4)  op_set_f_i(instr);
    else if (instr_H8 == 0xF5)  op_rst_f_i(instr);
    else if (instr == 0x0FDB)   op_inc_sp(instr);
    else if (instr == 0x0FCB)   op_dec_sp(instr);
    else if (instr_H10 == 0x3F0) op_push_r(instr);
    else if (instr == 0x0FC4)   op_push_xp(instr);
    else if (instr == 0x0FC5)   op_push_xh(instr);
    else if (instr == 0x0FC6)   op_push_xl(instr);
    else if (instr == 0x0FC7)   op_push_yp(instr);
    else if (instr == 0x0FC8)   op_push_yh(instr);
    else if (instr == 0x0FC9)   op_push_yl(instr);
    else if (instr == 0x0FCA)   op_push_f(instr);
    else if (instr_H10 == 0x3F4) op_pop_r(instr);
    else if (instr == 0x0FD4)   op_pop_xp(instr);
    else if (instr == 0x0FD5)   op_pop_xh(instr);
    else if (instr == 0x0FD6)   op_pop_xl(instr);
    else if (instr == 0x0FD7)   op_pop_yp(instr);
    else if (instr == 0x0FD8)   op_pop_yh(instr);
    else if (instr == 0x0FD9)   op_pop_yl(instr);
    else if (instr == 0x0FDA)   op_pop_f(instr);
    else if (instr_H10 == 0x3F8) op_ld_sph_r(instr);
    else if (instr_H10 == 0x3FC) op_ld_spl_r(instr);
    else if (instr_H10 == 0x3F9) op_ld_r_sph(instr);
    else if (instr_H10 == 0x3FD) op_ld_r_spl(instr);
    else if ((instr & 0x0FCF) == 0x0D0F)  op_not_r(instr);
    else if (instr_H6 == 0x30)  op_add_r_i(instr);
    else if (instr_H8 == 0xA8)  op_add_r_q(instr);
    else if (instr_H6 == 0x31)  op_adc_r_i(instr);
    else if (instr_H8 == 0xA9)  op_adc_r_q(instr);
    else if (instr_H8 == 0xAA)  op_sub_r_q(instr);
    else if (instr_H6 == 0x35)  op_sbc_r_i(instr);
    else if (instr_H8 == 0xAB)  op_sbc_r_q(instr);
    else if (instr_H6 == 0x32)  op_and_r_i(instr);
    else if (instr_H8 == 0xAC)  op_and_r_q(instr);
    else if (instr_H6 == 0x33)  op_or_r_i(instr);
    else if (instr_H8 == 0xAD)  op_or_r_q(instr);
    else if (instr_H6 == 0x34)  op_xor_r_i(instr);
    else if (instr_H8 == 0xAE)  op_xor_r_q(instr);
    else if (instr_H6 == 0x37)  op_cp_r_i(instr);
    else if (instr_H8 == 0xF0)  op_cp_r_q(instr);
    else if (instr_H6 == 0x36)  op_fan_r_i(instr);
    else if (instr_H8 == 0xF1)  op_fan_r_q(instr);
    else if (instr_H8 == 0xAF)  op_rlc_r(instr);
    else if (instr_H10 == 0x3A3)  op_rrc_r(instr);
    else if (instr_H8 == 0xF6)  op_inc_mn(instr);
    else if (instr_H8 == 0xF7)  op_dec_mn(instr);
    else if (instr_H10 == 0x3CA)  op_acpx_mx_r(instr);
    else if (instr_H10 == 0x3CB)  op_acpy_my_r(instr);
    else if (instr_H10 == 0x3CE)  op_scpx_mx_r(instr);
    else if (instr_H10 == 0x3CF)  op_scpy_my_r(instr);
    else std::printf("%03XH: Not Implemented\n", instr);

    if (record_history) {
        history[history_pos].opcode = instr;
        history_pos = (history_pos + 1) % history_size;
    }

    // NBP/NPP only hold a PSET value for the one instruction that follows it.
    // Otherwise they track the current bank/page, so jumps stay on this page.
    if (instr_H7 != 0x72) {
        reg.PC.tick();
    }
    after_pset = instr_H7 == 0x72;
}
