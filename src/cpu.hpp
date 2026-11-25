#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <stdexcept>

// Defining a 4-bit int type for clarity... but it's really 8 bits
using uint4_t = std::uint8_t;

struct ProgramCounter {
    bool NBP;
    bool PCB;
    uint4_t NPP;
    uint4_t PCP;
    uint8_t PCS;

    void tick() {
        if (NBP && NPP > 0x7) {
            throw std::runtime_error("Next PC is out-of-bounds");
        }

        NBP = PCB;
        NPP = PCP;
    }

    void reset() {
        NBP = PCB = false;
        NPP = PCP = 0x1;
    }

    void call(uint4_t page, uint8_t step) {
        PCP = page;
        PCS = step;
    }

    void jump(uint8_t step) {
        PCB = NBP;
        PCP = NPP;
        PCS = step;
    }

    void advance() {
        PCS++;
        if (PCS == 0) {
            PCP++;
            if (PCB == false && PCP > 0xF) {
                PCP = 0;
                PCB = true;
            } else if (PCB == true && PCP > 0x7) {
                PCP = 0;
                PCB = false;
            }
        }
    }
};

struct Registers {
    uint4_t A = 0;
    uint4_t B = 0;
    uint4_t RP = 0;
    std::uint8_t XHL = 0;
    std::uint8_t YHL = 0;
    uint4_t XP = 0;
    uint4_t YP = 0;
    std::uint8_t SP = 0;
    ProgramCounter PC{};
};

struct Flags {
    bool I = false;
    bool D = false;
    bool Z = false;
    bool C = false;

    // Pack into the IDZC nibble layout used by PUSH F / POP F
    constexpr uint4_t pack() const {
        return (I ? 0x8 : 0x0) |
               (D ? 0x4 : 0x0) |
               (Z ? 0x2 : 0x0) |
               (C ? 0x1 : 0x0);
    }

    constexpr void unpack(uint4_t f) {
        I = f & 0x8;
        D = f & 0x4;
        Z = f & 0x2;
        C = f & 0x1;
    }
};

// A RAM page is indexed [H][L], i.e. [step >> 4][step & 0xF]
template <std::size_t Rows>
using RamPage = std::array<std::array<uint4_t, 16>, Rows>;

struct Ram {
    RamPage<16> page0{};
    RamPage<16> page1{};
    RamPage<8> page2{};
    RamPage<16> pageE{};
    RamPage<16> pageF{};
};

using RomPage = std::array<std::uint16_t, 0x100>;

struct Rom {
    std::array<RomPage, 16> bank0{};
    std::array<RomPage, 8> bank1{};
};

struct LcdFrame {
    static constexpr uint8_t width = 32;
    static constexpr uint8_t height = 16;
    static constexpr uint8_t num_icons = 8;
    
    std::array<std::uint32_t, height> screen{};
    
    std::array<bool, num_icons> icons{};

    uint8_t contrast = 50;

    bool pixel(int x, int y) const { return (screen[y] >> x) & 1; }
};

struct RomAddr {
    std::uint8_t step = 0;
    uint4_t page = 0;
    bool bank = false;

    void advance() {
        step++;
        if (step == 0) {
            page++;
        }
        if (!bank && page == 16) {
            bank = true;
            page = 0;
        }
        if (bank && page == 8) {
            bank = false;
            page = 0;
        }
    }
};

// One executed instruction, as recorded for the core monitor
struct InstrRecord {
    bool bank = false;
    uint4_t page = 0;
    std::uint8_t step = 0;
    std::uint16_t opcode = 0;
    std::array<char, 24> text{};  // mnemonic, e.g. "LD A, MX"
};

struct IoReg { uint4_t h, l, mask; };

// Hidden timer state that isn't visible in the I/O registers (cpu_timer.cpp)
struct TimerState {
    std::uint8_t osc_div = 0;  // OSC1 prescaler, wraps at 128 (-> 256 Hz)
    std::uint8_t sw_div = 0;   // 256 Hz ticks since the stopwatch's last SWL count
};

class Cpu {
public:
    // Loads the ROM image. Throws runtime_error if the file can't be opened.
    explicit Cpu(const std::string& rom_path);

    void tick();

    Flags flags;
    Registers reg;
    Rom rom;
    Ram ram;
    std::uint8_t clock_wait = 0;
    bool halt = false;
    bool after_pset = false;  // last instruction was PSET; interrupts wait one more
    TimerState timers;

    // Keep a ring buffer of recently executed instructions (off by default).
    // history[history_pos] is the oldest entry / the next slot to be written.
    bool record_history = false;
    static constexpr std::size_t history_size = 32;
    std::array<InstrRecord, history_size> history{};
    std::size_t history_pos = 0;

    // Execution counters
    std::uint64_t instr_count = 0;  // instructions executed
    std::uint64_t halt_ticks = 0;   // ticks spent in HALT

    // Decodes the display RAM into an LCD frame (cpu_lcd.cpp)
    void lcd_frame(LcdFrame* frame) const;

private:
    // printf-style instruction log; stores the mnemonic in the history if
    // `record_history` is set, otherwise does nothing.
    // The attribute keeps gcc/clang's format-string checking.
    [[gnu::format(printf, 2, 3)]] void log_instr(const char* fmt, ...);

    // Nibble helpers for the 8-bit XHL / YHL / SP registers
    static constexpr uint4_t hi(std::uint8_t v) { return v >> 4; }
    static constexpr uint4_t lo(std::uint8_t v) { return v & 0xF; }
    static constexpr std::uint8_t with_hi(std::uint8_t v, uint4_t n) { return ((n & 0xF) << 4) | (v & 0x0F); }
    static constexpr std::uint8_t with_lo(std::uint8_t v, uint4_t n) { return (v & 0xF0) | (n & 0xF); }

    // Names for the 2-bit r/q register operands, used in the instruction logs
    static constexpr std::array<const char*, 4> reg_names = {"A", "B", "MX", "MY"};

    // RAM read/write
    void mem_write(uint4_t page, std::uint8_t step, uint4_t val);
    uint4_t mem_read(uint4_t page, std::uint8_t step);

    // Register read/write
    void reg_write(uint4_t reg_id, uint4_t val);
    uint4_t reg_read(uint4_t reg_id);

    // Interrupts (cpu_int.cpp). Takes a pending, enabled interrupt if there
    // is one, returning true if it did.
    bool service_interrupt();

    // Timers (cpu_timer.cpp)
    void timers_tick();
    void timers_io_write(uint4_t h, uint4_t l);
    void clock_timer_set(std::uint8_t tm);
    void stopwatch_step();
    void prog_timer_step();

    // IO register helper
    uint4_t io_read(IoReg r) const;
    void io_write(IoReg r, bool v);

    // 8-bit value stored as two I/O nibbles, low nibble at [h][l]
    std::uint8_t io_read8(uint4_t h, uint4_t l) const;
    void io_write8(uint4_t h, uint4_t l, std::uint8_t v);

    // Instruction lookup
    std::uint16_t prefetch();

    // Stack ops
    void stack_push(uint4_t val);
    uint4_t stack_pop();

    // PC helpers shared by the jump/call/return instructions
    void jump(std::uint8_t step);
    void push_pc();
    void pop_pc();

    // Arithmetic Logic Unit
    uint4_t alu_add(uint4_t a, uint4_t b, bool carry_in = false, bool decimal = true);
    uint4_t alu_sub(uint4_t a, uint4_t b, bool borrow_in = false, bool decimal = true);
    uint4_t alu_and(uint4_t a, uint4_t b);
    uint4_t alu_or(uint4_t a, uint4_t b);
    uint4_t alu_xor(uint4_t a, uint4_t b);
    uint4_t alu_not(uint4_t a);

    // ---- Instructions ----
    // Each takes the full 12-bit opcode and decodes its own operands.

    // Branch (cpu_branch.cpp)
    void op_pset(std::uint16_t instr);
    void op_jp(std::uint16_t instr);
    void op_jp_c(std::uint16_t instr);
    void op_jp_nc(std::uint16_t instr);
    void op_jp_z(std::uint16_t instr);
    void op_jp_nz(std::uint16_t instr);
    void op_jpba(std::uint16_t instr);
    void op_call(std::uint16_t instr);
    void op_calz(std::uint16_t instr);
    void op_ret(std::uint16_t instr);
    void op_rets(std::uint16_t instr);
    void op_retd(std::uint16_t instr);

    // System control (cpu_sys.cpp)
    void op_nop5(std::uint16_t instr);
    void op_nop7(std::uint16_t instr);
    void op_halt(std::uint16_t instr);

    // Index operation (cpu_idx.cpp)
    void op_inc_x(std::uint16_t instr);
    void op_inc_y(std::uint16_t instr);
    void op_ld_x_i(std::uint16_t instr);
    void op_ld_y_i(std::uint16_t instr);
    void op_ld_xp_r(std::uint16_t instr);
    void op_ld_xh_r(std::uint16_t instr);
    void op_ld_xl_r(std::uint16_t instr);
    void op_ld_yp_r(std::uint16_t instr);
    void op_ld_yh_r(std::uint16_t instr);
    void op_ld_yl_r(std::uint16_t instr);
    void op_ld_r_xp(std::uint16_t instr);
    void op_ld_r_xh(std::uint16_t instr);
    void op_ld_r_xl(std::uint16_t instr);
    void op_ld_r_yp(std::uint16_t instr);
    void op_ld_r_yh(std::uint16_t instr);
    void op_ld_r_yl(std::uint16_t instr);
    void op_adc_xh_i(std::uint16_t instr);
    void op_adc_xl_i(std::uint16_t instr);
    void op_adc_yh_i(std::uint16_t instr);
    void op_adc_yl_i(std::uint16_t instr);
    void op_cp_xh_i(std::uint16_t instr);
    void op_cp_xl_i(std::uint16_t instr);
    void op_cp_yh_i(std::uint16_t instr);
    void op_cp_yl_i(std::uint16_t instr);

    // Data transfer (cpu_data.cpp)
    void op_ld_r_i(std::uint16_t instr);
    void op_ld_r_q(std::uint16_t instr);
    void op_ld_a_mn(std::uint16_t instr);
    void op_ld_b_mn(std::uint16_t instr);
    void op_ld_mn_a(std::uint16_t instr);
    void op_ld_mn_b(std::uint16_t instr);
    void op_ldpx_mx_i(std::uint16_t instr);
    void op_ldpx_r_q(std::uint16_t instr);
    void op_ldpy_my_i(std::uint16_t instr);
    void op_ldpy_r_q(std::uint16_t instr);
    void op_lbpx_mx_l(std::uint16_t instr);

    // Flag operation (cpu_flag.cpp)
    void op_scf(std::uint16_t instr);
    void op_rcf(std::uint16_t instr);
    void op_szf(std::uint16_t instr);
    void op_rzf(std::uint16_t instr);
    void op_sdf(std::uint16_t instr);
    void op_rdf(std::uint16_t instr);
    void op_ei(std::uint16_t instr);
    void op_di(std::uint16_t instr);
    void op_set_f_i(std::uint16_t instr);
    void op_rst_f_i(std::uint16_t instr);

    // Stack operation (cpu_stack.cpp)
    void op_inc_sp(std::uint16_t instr);
    void op_dec_sp(std::uint16_t instr);
    void op_push_r(std::uint16_t instr);
    void op_push_xp(std::uint16_t instr);
    void op_push_xh(std::uint16_t instr);
    void op_push_xl(std::uint16_t instr);
    void op_push_yp(std::uint16_t instr);
    void op_push_yh(std::uint16_t instr);
    void op_push_yl(std::uint16_t instr);
    void op_push_f(std::uint16_t instr);
    void op_pop_r(std::uint16_t instr);
    void op_pop_xp(std::uint16_t instr);
    void op_pop_xh(std::uint16_t instr);
    void op_pop_xl(std::uint16_t instr);
    void op_pop_yp(std::uint16_t instr);
    void op_pop_yh(std::uint16_t instr);
    void op_pop_yl(std::uint16_t instr);
    void op_pop_f(std::uint16_t instr);
    void op_ld_sph_r(std::uint16_t instr);
    void op_ld_spl_r(std::uint16_t instr);
    void op_ld_r_sph(std::uint16_t instr);
    void op_ld_r_spl(std::uint16_t instr);

    // Arithmetic and logic (cpu_alu.cpp)
    void op_add_r_i(std::uint16_t instr);
    void op_add_r_q(std::uint16_t instr);
    void op_adc_r_i(std::uint16_t instr);
    void op_adc_r_q(std::uint16_t instr);
    void op_sub_r_q(std::uint16_t instr);
    void op_sbc_r_i(std::uint16_t instr);
    void op_sbc_r_q(std::uint16_t instr);
    void op_and_r_i(std::uint16_t instr);
    void op_and_r_q(std::uint16_t instr);
    void op_or_r_i(std::uint16_t instr);
    void op_or_r_q(std::uint16_t instr);
    void op_xor_r_i(std::uint16_t instr);
    void op_xor_r_q(std::uint16_t instr);
    void op_cp_r_i(std::uint16_t instr);
    void op_cp_r_q(std::uint16_t instr);
    void op_fan_r_i(std::uint16_t instr);
    void op_fan_r_q(std::uint16_t instr);
    void op_rlc_r(std::uint16_t instr);
    void op_rrc_r(std::uint16_t instr);
    void op_inc_mn(std::uint16_t instr);
    void op_dec_mn(std::uint16_t instr);
    void op_acpx_mx_r(std::uint16_t instr);
    void op_acpy_my_r(std::uint16_t instr);
    void op_scpx_mx_r(std::uint16_t instr);
    void op_scpy_my_r(std::uint16_t instr);
    void op_not_r(std::uint16_t instr);
};
