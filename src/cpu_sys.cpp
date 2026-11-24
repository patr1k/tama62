// System control instructions: NOP5, NOP7, HALT

#include "cpu.hpp"

#include <cstdio>

// NOP5: 1111 1111 1011
void Cpu::op_nop5(std::uint16_t) {
    log_instr("NOP5\n");
    clock_wait = 4;
}

// NOP7: 1111 1111 1111
void Cpu::op_nop7(std::uint16_t) {
    log_instr("NOP7\n");
    clock_wait = 6;
}

// HALT: 1111 1111 1000
void Cpu::op_halt(std::uint16_t) {
    log_instr("HALT\n");
    halt = true;
    clock_wait = 4;
}
