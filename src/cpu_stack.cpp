// Stack operation instructions: PUSH, POP, INC/DEC SP and SP loads

#include "cpu.hpp"

#include <cstdio>

// INC SP: 1111 1101 1011
void Cpu::op_inc_sp(std::uint16_t) {
    log_instr("INC SP\n");
    reg.SP++;
    clock_wait = 4;
}

// DEC SP: 1111 1100 1011
void Cpu::op_dec_sp(std::uint16_t) {
    log_instr("DEC SP\n");
    reg.SP--;
    clock_wait = 4;
}

// PUSH r: 1111 1100 00rr
void Cpu::op_push_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("PUSH %s\n", reg_names[r]);
    stack_push(reg_read(r));
    clock_wait = 4;
}

// PUSH XP: 1111 1100 0100
void Cpu::op_push_xp(std::uint16_t) {
    log_instr("PUSH XP\n");
    stack_push(reg.XP);
    clock_wait = 4;
}

// PUSH XH: 1111 1100 0101
void Cpu::op_push_xh(std::uint16_t) {
    log_instr("PUSH XH\n");
    stack_push(hi(reg.XHL));
    clock_wait = 4;
}

// PUSH XL: 1111 1100 0110
void Cpu::op_push_xl(std::uint16_t) {
    log_instr("PUSH XL\n");
    stack_push(lo(reg.XHL));
    clock_wait = 4;
}

// PUSH YP: 1111 1100 0111
void Cpu::op_push_yp(std::uint16_t) {
    log_instr("PUSH YP\n");
    stack_push(reg.YP);
    clock_wait = 4;
}

// PUSH YH: 1111 1100 1000
void Cpu::op_push_yh(std::uint16_t) {
    log_instr("PUSH YH\n");
    stack_push(hi(reg.YHL));
    clock_wait = 4;
}

// PUSH YL: 1111 1100 1001
void Cpu::op_push_yl(std::uint16_t) {
    log_instr("PUSH YL\n");
    stack_push(lo(reg.YHL));
    clock_wait = 4;
}

// PUSH F: 1111 1100 1010
void Cpu::op_push_f(std::uint16_t) {
    log_instr("PUSH F\n");
    stack_push(flags.pack());
    clock_wait = 4;
}

// POP r: 1111 1101 00rr
void Cpu::op_pop_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("POP %s\n", reg_names[r]);
    reg_write(r, stack_pop());
    clock_wait = 4;
}

// POP XP: 1111 1101 0100
void Cpu::op_pop_xp(std::uint16_t) {
    log_instr("POP XP\n");
    reg.XP = stack_pop();
    clock_wait = 4;
}

// POP XH: 1111 1101 0101
void Cpu::op_pop_xh(std::uint16_t) {
    log_instr("POP XH\n");
    reg.XHL = with_hi(reg.XHL, stack_pop());
    clock_wait = 4;
}

// POP XL: 1111 1101 0110
void Cpu::op_pop_xl(std::uint16_t) {
    log_instr("POP XL\n");
    reg.XHL = with_lo(reg.XHL, stack_pop());
    clock_wait = 4;
}

// POP YP: 1111 1101 0111
void Cpu::op_pop_yp(std::uint16_t) {
    log_instr("POP YP\n");
    reg.YP = stack_pop();
    clock_wait = 4;
}

// POP YH: 1111 1101 1000
void Cpu::op_pop_yh(std::uint16_t) {
    log_instr("POP YH\n");
    reg.YHL = with_hi(reg.YHL, stack_pop());
    clock_wait = 4;
}

// POP YL: 1111 1101 1001
void Cpu::op_pop_yl(std::uint16_t) {
    log_instr("POP YL\n");
    reg.YHL = with_lo(reg.YHL, stack_pop());
    clock_wait = 4;
}

// POP F: 1111 1101 1010
void Cpu::op_pop_f(std::uint16_t) {
    log_instr("POP F\n");
    flags.unpack(stack_pop());
    clock_wait = 4;
}

// LD SPH, r: 1111 1110 00rr
void Cpu::op_ld_sph_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD SPH, %s\n", reg_names[r]);
    reg.SP = with_hi(reg.SP, reg_read(r));
    clock_wait = 4;
}

// LD SPL, r: 1111 1111 00rr
void Cpu::op_ld_spl_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD SPL, %s\n", reg_names[r]);
    reg.SP = with_lo(reg.SP, reg_read(r));
    clock_wait = 4;
}

// LD r, SPH: 1111 1110 01rr
void Cpu::op_ld_r_sph(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, SPH\n", reg_names[r]);
    reg_write(r, hi(reg.SP));
    clock_wait = 4;
}

// LD r, SPL: 1111 1111 01rr
void Cpu::op_ld_r_spl(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, SPL\n", reg_names[r]);
    reg_write(r, lo(reg.SP));
    clock_wait = 4;
}
