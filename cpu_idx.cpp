// Index operation instructions: X / Y register loads, increments, ADC and CP

#include "cpu.hpp"

#include <cstdio>

// INC X: 1110 1110 0000
void Cpu::op_inc_x(std::uint16_t) {
    log_instr("INC X\n");
    reg.XHL++;
    clock_wait = 4;
}

// INC Y: 1110 1111 0000
void Cpu::op_inc_y(std::uint16_t) {
    log_instr("INC Y\n");
    reg.YHL++;
    clock_wait = 4;
}

// LD X, x: 1011 xxxx xxxx
void Cpu::op_ld_x_i(std::uint16_t instr) {
    std::uint8_t x = instr & 0xFF;
    log_instr("LD X, %02XH\n", x);
    reg.XHL = x;
    clock_wait = 4;
}

// LD Y, y: 1000 yyyy yyyy
void Cpu::op_ld_y_i(std::uint16_t instr) {
    std::uint8_t y = instr & 0xFF;
    log_instr("LD Y, %02XH\n", y);
    reg.YHL = y;
    clock_wait = 4;
}

// LD XP, r: 1110 1000 00rr
void Cpu::op_ld_xp_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD XP, %s\n", reg_names[r]);
    reg.XP = reg_read(r);
    clock_wait = 4;
}

// LD XH, r: 1110 1000 01rr
void Cpu::op_ld_xh_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD XH, %s\n", reg_names[r]);
    reg.XHL = with_hi(reg.XHL, reg_read(r));
    clock_wait = 4;
}

// LD XL, r: 1110 1000 10rr
void Cpu::op_ld_xl_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD XL, %s\n", reg_names[r]);
    reg.XHL = with_lo(reg.XHL, reg_read(r));
    clock_wait = 4;
}

// LD YP, r: 1110 1001 00rr
void Cpu::op_ld_yp_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD YP, %s\n", reg_names[r]);
    reg.YP = reg_read(r);
    clock_wait = 4;
}

// LD YH, r: 1110 1001 01rr
void Cpu::op_ld_yh_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD YH, %s\n", reg_names[r]);
    reg.YHL = with_hi(reg.YHL, reg_read(r));
    clock_wait = 4;
}

// LD YL, r: 1110 1001 10rr
void Cpu::op_ld_yl_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD YL, %s\n", reg_names[r]);
    reg.YHL = with_lo(reg.YHL, reg_read(r));
    clock_wait = 4;
}

// LD r, XP: 1110 1010 00rr
void Cpu::op_ld_r_xp(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, XP\n", reg_names[r]);
    reg_write(r, reg.XP);
    clock_wait = 4;
}

// LD r, XH: 1110 1010 01rr
void Cpu::op_ld_r_xh(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, XH\n", reg_names[r]);
    reg_write(r, hi(reg.XHL));
    clock_wait = 4;
}

// LD r, XL: 1110 1010 10rr
void Cpu::op_ld_r_xl(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, XL\n", reg_names[r]);
    reg_write(r, lo(reg.XHL));
    clock_wait = 4;
}

// LD r, YP: 1110 1011 00rr
void Cpu::op_ld_r_yp(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, YP\n", reg_names[r]);
    reg_write(r, reg.YP);
    clock_wait = 4;
}

// LD r, YH: 1110 1011 01rr
void Cpu::op_ld_r_yh(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, YH\n", reg_names[r]);
    reg_write(r, hi(reg.YHL));
    clock_wait = 4;
}

// LD r, YL: 1110 1011 10rr
void Cpu::op_ld_r_yl(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("LD %s, YL\n", reg_names[r]);
    reg_write(r, lo(reg.YHL));
    clock_wait = 4;
}

// ADC XH, i: 1010 0000 iiii
void Cpu::op_adc_xh_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("ADC XH, %XH\n", i);
    reg.XHL = with_hi(reg.XHL, alu_add(hi(reg.XHL), i, true, false));
    clock_wait = 6;
}

// ADC XL, i: 1010 0001 iiii
void Cpu::op_adc_xl_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("ADC XL, %XH\n", i);
    reg.XHL = with_lo(reg.XHL, alu_add(lo(reg.XHL), i, true, false));
    clock_wait = 6;
}

// ADC YH, i: 1010 0010 iiii
void Cpu::op_adc_yh_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("ADC YH, %XH\n", i);
    reg.YHL = with_hi(reg.YHL, alu_add(hi(reg.YHL), i, true, false));
    clock_wait = 6;
}

// ADC YL, i: 1010 0011 iiii
void Cpu::op_adc_yl_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("ADC YL, %XH\n", i);
    reg.YHL = with_lo(reg.YHL, alu_add(lo(reg.YHL), i, true, false));
    clock_wait = 6;
}

// CP XH, i: 1010 0100 iiii
void Cpu::op_cp_xh_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("CP XH, %XH\n", i);
    alu_sub(hi(reg.XHL), i, false, false);
    clock_wait = 6;
}

// CP XL, i: 1010 0101 iiii
void Cpu::op_cp_xl_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("CP XL, %XH\n", i);
    alu_sub(lo(reg.XHL), i, false, false);
    clock_wait = 6;
}

// CP YH, i: 1010 0110 iiii
void Cpu::op_cp_yh_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("CP YH, %XH\n", i);
    alu_sub(hi(reg.YHL), i, false, false);
    clock_wait = 6;
}

// CP YL, i: 1010 0111 iiii
void Cpu::op_cp_yl_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("CP YL, %XH\n", i);
    alu_sub(lo(reg.YHL), i, false, false);
    clock_wait = 6;
}
