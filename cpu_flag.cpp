// Flag operation instructions: SET F, RST F and their named aliases

#include "cpu.hpp"

#include <cstdio>

// SCF (SET F, 1H): 1111 0100 0001
void Cpu::op_scf(std::uint16_t) {
    log_instr("SCF\n");
    flags.C = true;
    clock_wait = 6;
}

// RCF (RST F, EH): 1111 0101 1110
void Cpu::op_rcf(std::uint16_t) {
    log_instr("RCF\n");
    flags.C = false;
    clock_wait = 6;
}

// SZF (SET F, 2H): 1111 0100 0010
void Cpu::op_szf(std::uint16_t) {
    log_instr("SZF\n");
    flags.Z = true;
    clock_wait = 6;
}

// RZF (RST F, DH): 1111 0101 1101
void Cpu::op_rzf(std::uint16_t) {
    log_instr("RZF\n");
    flags.Z = false;
    clock_wait = 6;
}

// SDF (SET F, 4H): 1111 0100 0100
void Cpu::op_sdf(std::uint16_t) {
    log_instr("SDF\n");
    flags.D = true;
    clock_wait = 6;
}

// RDF (RST F, BH): 1111 0101 1011
void Cpu::op_rdf(std::uint16_t) {
    log_instr("RDF\n");
    flags.D = false;
    clock_wait = 6;
}

// EI (SET F, 8H): 1111 0100 1000
void Cpu::op_ei(std::uint16_t) {
    log_instr("EI\n");
    flags.I = true;
    clock_wait = 6;
}

// DI (RST F, 7H): 1111 0101 0111
void Cpu::op_di(std::uint16_t) {
    log_instr("DI\n");
    flags.I = false;
    clock_wait = 6;
}

// SET F, i: 1111 0100 iiii
void Cpu::op_set_f_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("SET F, %XH\n", i);
    flags.unpack(flags.pack() | i);
    clock_wait = 6;
}

// RST F, i: 1111 0101 iiii
void Cpu::op_rst_f_i(std::uint16_t instr) {
    uint4_t i = instr & 0xF;
    log_instr("RST F, %XH\n", i);
    flags.unpack(flags.pack() & i);
    clock_wait = 6;
}
