// Data transfer instructions: LD, LDPX, LDPY, LBPX

#include "cpu.hpp"

#include <cstdio>

// LD r, i: 1110 00rr iiii
void Cpu::op_ld_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("LD %s, %XH\n", reg_names[r], i);
    reg_write(r, i);
    clock_wait = 4;
}

// LD r, q: 1110 1100 rrqq
void Cpu::op_ld_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("LD %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, reg_read(q));
    clock_wait = 4;
}

// LD A, Mn: 1111 1010 nnnn
void Cpu::op_ld_a_mn(std::uint16_t instr) {
    uint4_t n = instr & 0xF;
    log_instr("LD A, M%X\n", n);
    reg.A = mem_read(0, n);
    clock_wait = 4;
}

// LD B, Mn: 1111 1011 nnnn
void Cpu::op_ld_b_mn(std::uint16_t instr) {
    uint4_t n = instr & 0xF;
    log_instr("LD B, M%X\n", n);
    reg.B = mem_read(0, n);
    clock_wait = 4;
}

// LD Mn, A: 1111 1000 nnnn
void Cpu::op_ld_mn_a(std::uint16_t instr) {
    uint4_t n = instr & 0xF;
    log_instr("LD M%X, A\n", n);
    mem_write(0, n, reg.A);
    clock_wait = 4;
}

// LD Mn, B: 1111 1001 nnnn
void Cpu::op_ld_mn_b(std::uint16_t instr) {
    uint4_t n = instr & 0xF;
    log_instr("LD M%X, B\n", n);
    mem_write(0, n, reg.B);
    clock_wait = 4;
}

// LDPX MX, i: 1110 0110 iiii
void Cpu::op_ldpx_mx_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("LDPX MX, %XH\n", i);
    mem_write(reg.XP, reg.XHL, i);
    reg.XHL++;
    clock_wait = 4;
}

// LDPX r, q: 1110 1110 rrqq
void Cpu::op_ldpx_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("LDPX %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, reg_read(q));
    reg.XHL++;
    clock_wait = 4;
}

// LDPY MY, i: 1110 0111 iiii
void Cpu::op_ldpy_my_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("LDPY MY, %XH\n", i);
    mem_write(reg.YP, reg.YHL, i);
    reg.YHL++;
    clock_wait = 4;
}

// LDPY r, q: 1110 1111 rrqq
void Cpu::op_ldpy_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("LDPY %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, reg_read(q));
    reg.YHL++;
    clock_wait = 4;
}

// LBPX MX, l: 1001 llll llll
void Cpu::op_lbpx_mx_l(std::uint16_t instr) {
    std::uint8_t l = instr & 0xFF;
    log_instr("LBPX MX, %02XH\n", l);
    mem_write(reg.XP, reg.XHL + 0, lo(l));
    mem_write(reg.XP, reg.XHL + 1, hi(l));
    reg.XHL += 2;
    clock_wait = 4;
}
