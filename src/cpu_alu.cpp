// Arithmetic and logic: the ALU helpers plus ADD, ADC, SUB, SBC, AND, OR, XOR, CP, FAN

#include "cpu.hpp"

#include <cstdio>

// ---- ALU helpers ----

uint4_t Cpu::alu_add(uint4_t a, uint4_t b, bool carry_in, bool decimal) {
    std::uint8_t result = a + b + ((carry_in && flags.C) ? 1 : 0);

    if (decimal && flags.D) {
        if (result >= 10) {
            result += 6;
            flags.C = true;
        } else {
            flags.C = false;
        }
    } else {
        flags.C = result > 0xF;
    }

    flags.Z = (result & 0xF) == 0;

    return result & 0xF;
}

uint4_t Cpu::alu_sub(uint4_t a, uint4_t b, bool borrow_in, bool decimal) {
    int result = static_cast<int>(a) - static_cast<int>(b) - ((borrow_in && flags.C) ? 1 : 0);

    // C is set if subtraction requires borrow
    flags.C = result < 0;

    if (decimal && flags.D && result < 0) {
        result += 10; // BCD correction
    }

    uint4_t value = result & 0xF;
    flags.Z = value == 0;

    return value;
}

uint4_t Cpu::alu_and(uint4_t a, uint4_t b) {
    uint4_t result = a & b;

    flags.Z = (result & 0xF) == 0;

    return result & 0xF;
}

uint4_t Cpu::alu_or(uint4_t a, uint4_t b) {
    uint4_t result = a | b;

    flags.Z = (result & 0xF) == 0;

    return result & 0xF;
}

uint4_t Cpu::alu_xor(uint4_t a, uint4_t b) {
    uint4_t result = a ^ b;

    flags.Z = (result & 0xF) == 0;

    return result & 0xF;
}

uint4_t Cpu::alu_not(uint4_t a) {
    uint4_t result = ~a;

    flags.Z = (result & 0xF) == 0;

    return result & 0xF;
}

// ---- Instructions ----

// ADD r, i: 1100 00rr iiii
void Cpu::op_add_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("ADD %s, %XH\n", reg_names[r], i);
    reg_write(r, alu_add(reg_read(r), i));
    clock_wait = 6;
}

// ADD r, q: 1010 1000 rrqq
void Cpu::op_add_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("ADD %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_add(reg_read(r), reg_read(q)));
    clock_wait = 6;
}

// ADC r, i: 1100 01rr iiii
void Cpu::op_adc_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("ADC %s, %XH\n", reg_names[r], i);
    reg_write(r, alu_add(reg_read(r), i, true));
    clock_wait = 6;
}

// ADC r, q: 1010 1001 rrqq
void Cpu::op_adc_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("ADC %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_add(reg_read(r), reg_read(q), true));
    clock_wait = 6;
}

// SUB r, q: 1010 1010 rrqq
void Cpu::op_sub_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("SUB %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_sub(reg_read(r), reg_read(q)));
    clock_wait = 6;
}

// SBC r, i: 1101 01rr iiii
void Cpu::op_sbc_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("SBC %s, %XH\n", reg_names[r], i);
    reg_write(r, alu_sub(reg_read(r), i, true));
    clock_wait = 6;
}

// SBC r, q: 1010 1011 rrqq
void Cpu::op_sbc_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("SBC %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_sub(reg_read(r), reg_read(q), true));
    clock_wait = 6;
}

// AND r, i: 1100 10rr iiii
void Cpu::op_and_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("AND %s, %XH\n", reg_names[r], i);
    reg_write(r, alu_and(reg_read(r), i));
    clock_wait = 6;
}

// AND r, q: 1010 1100 rrqq
void Cpu::op_and_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("AND %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_and(reg_read(r), reg_read(q)));
    clock_wait = 6;
}

// OR r, i: 1100 11rr iiii
void Cpu::op_or_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("OR %s, %XH\n", reg_names[r], i);
    reg_write(r, alu_or(reg_read(r), i));
    clock_wait = 6;
}

// OR r, q: 1010 1101 rrqq
void Cpu::op_or_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("OR %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_or(reg_read(r), reg_read(q)));
    clock_wait = 6;
}

// XOR r, i: 1101 00rr iiii
void Cpu::op_xor_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("XOR %s, %XH\n", reg_names[r], i);
    reg_write(r, alu_xor(reg_read(r), i));
    clock_wait = 6;
}

// XOR r, q: 1010 1110 rrqq
void Cpu::op_xor_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("XOR %s, %s\n", reg_names[r], reg_names[q]);
    reg_write(r, alu_xor(reg_read(r), reg_read(q)));
    clock_wait = 6;
}

// CP r, i: 1101 11rr iiii
void Cpu::op_cp_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("CP %s, %XH\n", reg_names[r], i);
    alu_sub(reg_read(r), i, false, false);
    clock_wait = 6;
}

// CP r, q: 1111 0000 rrqq
void Cpu::op_cp_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("CP %s, %s\n", reg_names[r], reg_names[q]);
    alu_sub(reg_read(r), reg_read(q), false, false);
    clock_wait = 6;
}

// FAN r, i: 1101 10rr iiii
void Cpu::op_fan_r_i(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    uint4_t i = instr & 0xF;
    log_instr("FAN %s, %XH\n", reg_names[r], i);
    alu_and(reg_read(r), i);
    clock_wait = 6;
}

// FAN r, q: 1111 0001 rrqq
void Cpu::op_fan_r_q(std::uint16_t instr) {
    uint4_t r = (instr >> 2) & 0x3;
    uint4_t q = instr & 0x3;
    log_instr("FAN %s, %s\n", reg_names[r], reg_names[q]);
    alu_and(reg_read(r), reg_read(q));
    clock_wait = 6;
}

// RLC r: 1010 1111 rrrr
void Cpu::op_rlc_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("RLC %s\n", reg_names[r]);
    uint8_t result = reg_read(r) << 1;
    result |= flags.C ? 1 : 0;
    flags.C = result & 0x10;
    reg_write(r, result & 0xF);
    clock_wait = 6;
}

// RRC r: 1110 1000 11rr
void Cpu::op_rrc_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("RRC %s\n", reg_names[r]);
    uint8_t result = reg_read(r);
    bool carry_out = result & 0x1;
    result >>= 1;
    result |= flags.C ? 0x8 : 0;
    flags.C = carry_out;
    reg_write(r, result & 0xF);
    clock_wait = 4;
}

// INC Mn: 1111 0110 nnnn
void Cpu::op_inc_mn(std::uint16_t instr) {
    uint4_t n = instr & 0xF;
    log_instr("INC M%X\n", n);
    uint4_t result = alu_add(mem_read(0, n), 1, false, false);
    mem_write(0, n, result);
    clock_wait = 6;
}

// DEC Mn: 1111 0111 nnnn
void Cpu::op_dec_mn(std::uint16_t instr) {
    uint4_t n = instr & 0xF;
    log_instr("DEC M%X\n", n);
    uint4_t result = alu_sub(mem_read(0, n), 1, false, false);
    mem_write(0, n, result);
    clock_wait = 6;
}

// ACPX MX, r: 1111 0010 10rr
void Cpu::op_acpx_mx_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("ACPX MX, %s\n", reg_names[r]);
    uint4_t result = alu_add(mem_read(reg.XP, reg.XHL), reg_read(r), true);
    mem_write(reg.XP, reg.XHL, result);
    reg.XHL++;
    clock_wait = 6;
}

// ACPY MY, r: 1111 0010 11rr
void Cpu::op_acpy_my_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("ACPY MY, %s\n", reg_names[r]);
    uint4_t result = alu_add(mem_read(reg.YP, reg.YHL), reg_read(r), true);
    mem_write(reg.YP, reg.YHL, result);
    reg.YHL++;
    clock_wait = 6;
}

// SCPX MX, r: 1111 0011 10rr
void Cpu::op_scpx_mx_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("SCPX MX, %s\n", reg_names[r]);
    uint4_t result = alu_sub(mem_read(reg.XP, reg.XHL), reg_read(r), true);
    mem_write(reg.XP, reg.XHL, result);
    reg.XHL++;
    clock_wait = 6;
}

// SCPY MY, r: 1111 0011 11rr
void Cpu::op_scpy_my_r(std::uint16_t instr) {
    uint4_t r = instr & 0x3;
    log_instr("SCPY MY, %s\n", reg_names[r]);
    uint4_t result = alu_sub(mem_read(reg.YP, reg.YHL), reg_read(r), true);
    mem_write(reg.YP, reg.YHL, result);
    reg.YHL++;
    clock_wait = 6;
}

// NOT r: 1101 00rr 1111
void Cpu::op_not_r(std::uint16_t instr) {
    uint4_t r = (instr >> 4) & 0x3;
    log_instr("NOT %s\n", reg_names[r]);
    reg_write(r, alu_not(reg_read(r)));
    clock_wait = 6;
}