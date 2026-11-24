// Branch instructions: PSET, JP, CALL and RET variants

#include "cpu.hpp"

#include <cstdio>

// PSET p: 1110 010p pppp
void Cpu::op_pset(std::uint16_t instr) {
    log_instr("PSET %02XH\n", instr & 0x1F);
    reg.PC.NBP = instr & 0x0010;
    reg.PC.NPP = instr & 0x000F;
    clock_wait = 4;
}

// JP s: 0000 ssss ssss
void Cpu::op_jp(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("JP %02XH\n", s);
    jump(s);
    clock_wait = 4;
}

// JP C, s: 0010 ssss ssss
void Cpu::op_jp_c(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("JP C, %02XH\n", s);
    if (flags.C) jump(s);
    clock_wait = 4;
}

// JP NC, s: 0011 ssss ssss
void Cpu::op_jp_nc(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("JP NC, %02XH\n", s);
    if (!flags.C) jump(s);
    clock_wait = 4;
}

// JP Z, s: 0110 ssss ssss
void Cpu::op_jp_z(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("JP Z, %02XH\n", s);
    if (flags.Z) jump(s);
    clock_wait = 4;
}

// JP NZ, s: 0111 ssss ssss
void Cpu::op_jp_nz(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("JP NZ, %02XH\n", s);
    if (!flags.Z) jump(s);
    clock_wait = 4;
}

// JPBA: 1111 1110 1000
void Cpu::op_jpba(std::uint16_t) {
    log_instr("JPBA\n");
    jump((reg.B << 4) | reg.A);
    clock_wait = 4;
}

// CALL s: 0100 ssss ssss
void Cpu::op_call(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("CALL %02XH\n", s);
    push_pc();
    reg.PC.call(reg.PC.NPP, s);
    clock_wait = 6;
}

// CALZ s: 0101 ssss ssss
void Cpu::op_calz(std::uint16_t instr) {
    std::uint8_t s = instr & 0xFF;
    log_instr("CALZ %02XH\n", s);
    push_pc();
    reg.PC.call(0, s);
    clock_wait = 6;
}

// RET: 1111 1101 1111
void Cpu::op_ret(std::uint16_t) {
    log_instr("RET\n");
    pop_pc();
    clock_wait = 6;
}

// RETS: 1111 1101 1110
void Cpu::op_rets(std::uint16_t) {
    log_instr("RETS\n");
    pop_pc();
    reg.PC.advance();
    clock_wait = 11;
}

// RETD l: 0001 llll llll
void Cpu::op_retd(std::uint16_t instr) {
    std::uint8_t l = instr & 0xFF;
    log_instr("RETD %02XH\n", l);
    pop_pc();
    mem_write(reg.XP, reg.XHL, lo(l));
    reg.XHL++;
    mem_write(reg.XP, reg.XHL, hi(l));
    reg.XHL++;
    clock_wait = 11;
}
